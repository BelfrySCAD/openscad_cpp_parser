#include "openscad_cpp_parser/pretty_print.hpp"

#include "ast/format_utils.hpp"

#include <optional>
#include <tuple>
#include <utility>

namespace oscad {

namespace {

constexpr int kMultilineCharLimit = 80;

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string lstrip(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(i);
}

std::string strip(const std::string& s) {
    std::string l = lstrip(s);
    size_t end = l.size();
    while (end > 0 && (l[end - 1] == ' ' || l[end - 1] == '\t')) {
        --end;
    }
    return l.substr(0, end);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

bool anyContainsNewline(const std::vector<std::string>& items) {
    for (const auto& s : items) {
        if (s.find('\n') != std::string::npos) {
            return true;
        }
    }
    return false;
}

// True if `s`'s last line contains a `//` comment -- meaning it runs to
// end-of-line and nothing (a `,`, closing `)`/`]`, `;`, ...) may safely
// follow it inline. Used to force the multiline path (where
// fmtMultilineArgsGeneric puts each item and the closing bracket on their
// own lines) instead of the inline path, which would otherwise silently
// swallow whatever comes next into the comment on re-parse.
bool endsWithLineComment(const std::string& s) {
    size_t lastNl = s.rfind('\n');
    std::string lastLine = (lastNl == std::string::npos) ? s : s.substr(lastNl + 1);
    return lastLine.find("//") != std::string::npos;
}

bool anyEndsWithLineComment(const std::vector<std::string>& items) {
    for (const auto& s : items) {
        if (endsWithLineComment(s)) {
            return true;
        }
    }
    return false;
}

// Inserts `terminator` (e.g. "," or ";") right before any unterminated
// trailing `//` comment on `formatted`'s last line, instead of blindly
// appending after it (which would silently swallow the terminator into
// the comment on re-parse). No-op (plain append) when there's no trailing
// line comment.
std::string appendTerminatorSafely(const std::string& formatted, const std::string& terminator) {
    size_t lastNl = formatted.rfind('\n');
    size_t lineStart = (lastNl == std::string::npos) ? 0 : lastNl + 1;
    size_t commentPos = formatted.find("//", lineStart);
    if (commentPos == std::string::npos) {
        return formatted + terminator;
    }
    return formatted.substr(0, commentPos) + terminator + " " + formatted.substr(commentPos);
}

std::string joinComments(const std::vector<std::unique_ptr<CommentSpan>>& comments) {
    std::vector<std::string> parts;
    for (const auto& c : comments) {
        parts.push_back(c->toString());
    }
    return join(parts, " ");
}

struct BinaryOpView {
    const char* symbol;
    const Expression* left;
    const Expression* right;
};

std::optional<BinaryOpView> asBinaryOp(const ASTNode& node) {
    switch (node.kind()) {
        case NodeKind::AdditionOp: {
            auto& n = static_cast<const AdditionOp&>(node);
            return BinaryOpView{"+", n.left.get(), n.right.get()};
        }
        case NodeKind::SubtractionOp: {
            auto& n = static_cast<const SubtractionOp&>(node);
            return BinaryOpView{"-", n.left.get(), n.right.get()};
        }
        case NodeKind::MultiplicationOp: {
            auto& n = static_cast<const MultiplicationOp&>(node);
            return BinaryOpView{"*", n.left.get(), n.right.get()};
        }
        case NodeKind::DivisionOp: {
            auto& n = static_cast<const DivisionOp&>(node);
            return BinaryOpView{"/", n.left.get(), n.right.get()};
        }
        case NodeKind::ModuloOp: {
            auto& n = static_cast<const ModuloOp&>(node);
            return BinaryOpView{"%", n.left.get(), n.right.get()};
        }
        case NodeKind::ExponentOp: {
            auto& n = static_cast<const ExponentOp&>(node);
            return BinaryOpView{"^", n.left.get(), n.right.get()};
        }
        case NodeKind::BitwiseAndOp: {
            auto& n = static_cast<const BitwiseAndOp&>(node);
            return BinaryOpView{"&", n.left.get(), n.right.get()};
        }
        case NodeKind::BitwiseOrOp: {
            auto& n = static_cast<const BitwiseOrOp&>(node);
            return BinaryOpView{"|", n.left.get(), n.right.get()};
        }
        case NodeKind::BitwiseShiftLeftOp: {
            auto& n = static_cast<const BitwiseShiftLeftOp&>(node);
            return BinaryOpView{"<<", n.left.get(), n.right.get()};
        }
        case NodeKind::BitwiseShiftRightOp: {
            auto& n = static_cast<const BitwiseShiftRightOp&>(node);
            return BinaryOpView{">>", n.left.get(), n.right.get()};
        }
        case NodeKind::LogicalAndOp: {
            auto& n = static_cast<const LogicalAndOp&>(node);
            return BinaryOpView{"&&", n.left.get(), n.right.get()};
        }
        case NodeKind::LogicalOrOp: {
            auto& n = static_cast<const LogicalOrOp&>(node);
            return BinaryOpView{"||", n.left.get(), n.right.get()};
        }
        case NodeKind::EqualityOp: {
            auto& n = static_cast<const EqualityOp&>(node);
            return BinaryOpView{"==", n.left.get(), n.right.get()};
        }
        case NodeKind::InequalityOp: {
            auto& n = static_cast<const InequalityOp&>(node);
            return BinaryOpView{"!=", n.left.get(), n.right.get()};
        }
        case NodeKind::GreaterThanOp: {
            auto& n = static_cast<const GreaterThanOp&>(node);
            return BinaryOpView{">", n.left.get(), n.right.get()};
        }
        case NodeKind::GreaterThanOrEqualOp: {
            auto& n = static_cast<const GreaterThanOrEqualOp&>(node);
            return BinaryOpView{">=", n.left.get(), n.right.get()};
        }
        case NodeKind::LessThanOp: {
            auto& n = static_cast<const LessThanOp&>(node);
            return BinaryOpView{"<", n.left.get(), n.right.get()};
        }
        case NodeKind::LessThanOrEqualOp: {
            auto& n = static_cast<const LessThanOrEqualOp&>(node);
            return BinaryOpView{"<=", n.left.get(), n.right.get()};
        }
        default:
            return std::nullopt;
    }
}

bool isModuleInstantiationKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::ModularCall:
        case NodeKind::ModularFor:
        case NodeKind::ModularIntersectionFor:
        case NodeKind::ModularLet:
        case NodeKind::ModularEcho:
        case NodeKind::ModularAssert:
        case NodeKind::ModularIf:
        case NodeKind::ModularIfElse:
        case NodeKind::ModularModifierShowOnly:
        case NodeKind::ModularModifierHighlight:
        case NodeKind::ModularModifierBackground:
        case NodeKind::ModularModifierDisable:
            return true;
        default:
            return false;
    }
}

std::string fmtNode(const ASTNode& node, int indent, int w);
std::string fmtExpr(const ASTNode& expr, int indent, int w);
std::string fmtInst(const ASTNode& node, int indent, int w, const std::string& prefix);
std::string fmtListElem(const ASTNode& elem, int indent, int w);

std::string fmtAssign(const Assignment& a, int indent, int w) {
    return a.name->name + " = " + fmtExpr(*a.expr, indent, w);
}

std::string fmtArgument(const Argument& arg, int indent, int w) {
    if (auto* pa = dynamic_cast<const PositionalArgument*>(&arg)) {
        return fmtExpr(*pa->expr, indent, w);
    }
    if (auto* na = dynamic_cast<const NamedArgument*>(&arg)) {
        return na->name->name + "=" + fmtExpr(*na->expr, indent, w);
    }
    return arg.toString();
}

std::string fmtParameter(const ParameterDeclaration& param) {
    std::string parts;
    for (const auto& c : param.leadingComments) {
        parts += c->toString() + " ";
    }
    bool hasDefault = param.defaultValue && param.defaultValue->kind() != NodeKind::UndefinedLiteral;
    parts += param.name->name + (hasDefault ? "=" + param.defaultValue->toString() : "");
    for (const auto& c : param.trailingComments) {
        parts += " " + c->toString();
    }
    return parts;
}

std::string joinParams(const std::vector<std::unique_ptr<ParameterDeclaration>>& params) {
    std::vector<std::string> parts;
    for (const auto& p : params) {
        parts.push_back(fmtParameter(*p));
    }
    return join(parts, ", ");
}

std::string fmtMultilineArgsGeneric(const std::string& head, const std::vector<std::string>& formattedArgs, int indent,
                                     int w) {
    std::string innerPad(indent + w, ' ');
    std::string pad(indent, ' ');
    std::vector<std::string> lines;
    for (size_t i = 0; i < formattedArgs.size(); ++i) {
        std::string terminator = (i + 1 < formattedArgs.size()) ? "," : "";
        const std::string& item = formattedArgs[i];
        // A rendered arg beginning with a bare "//" line (from a leading
        // CommentLine on this value -- see fmtCommentedExpr) can't stay on
        // its own fresh line here: nothing would precede it there, so
        // re-parsing would misclassify it as a *standalone* comment
        // instead of staying attached to this argument, losing the
        // association (a real round-trip bug this fixes). Move that first
        // line onto the end of the previous argument's own line instead --
        // mirrors how fmtListComprehension already renders the same
        // situation for list elements. The remainder (if any) already
        // carries fmtCommentedExpr's own innerPad-width indent, baked in
        // when it joined that value onto its own continuation line, so it
        // must NOT be indented again here.
        if (startsWith(item, "//")) {
            size_t nl = item.find('\n');
            std::string commentLine = (nl == std::string::npos) ? item : item.substr(0, nl);
            std::string rest = (nl == std::string::npos) ? "" : item.substr(nl + 1);
            if (!lines.empty()) {
                lines.back() += "  " + commentLine;
            } else {
                lines.push_back(innerPad + commentLine);
            }
            if (!rest.empty()) {
                lines.push_back(appendTerminatorSafely(rest, terminator));
            } else {
                lines.back() = appendTerminatorSafely(lines.back(), terminator);
            }
            continue;
        }
        lines.push_back(innerPad + appendTerminatorSafely(item, terminator));
    }
    return head + "(\n" + join(lines, "\n") + "\n" + pad + ")";
}

std::string fmtBranch(const ASTNode& branch, int indent, int w) {
    if (branch.kind() == NodeKind::TernaryOp) {
        return fmtExpr(branch, indent + w, w);
    }
    return fmtExpr(branch, indent + w + 2, w);
}

std::string fmtTernaryChain(const TernaryOp& expr, int indent, int w) {
    std::string pad(indent, ' ');
    std::string innerPad(indent + w, ' ');
    std::vector<std::pair<const Expression*, const Expression*>> parts;
    const ASTNode* node = &expr;
    while (node->kind() == NodeKind::TernaryOp) {
        auto* t = static_cast<const TernaryOp*>(node);
        parts.emplace_back(t->condition.get(), t->trueExpr.get());
        node = t->falseExpr.get();
        if (auto* ce = dynamic_cast<const CommentedExpr*>(node)) {
            if (ce->expr->kind() == NodeKind::TernaryOp) {
                node = ce->expr.get();
            }
        }
    }
    const ASTNode* final = node;
    std::vector<std::string> lines;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string trueStr = fmtExpr(*parts[i].second, indent + w, w);
        std::string prefix = (i == 0) ? "" : (pad + ": ");
        lines.push_back(prefix + parts[i].first->toString() + " ?\n" + innerPad + trueStr);
    }
    lines.push_back(pad + ": " + fmtExpr(*final, indent + w, w));
    return join(lines, "\n");
}

std::string fmtTernary(const TernaryOp& expr, int indent, int w) {
    const ASTNode* falseInner = expr.falseExpr.get();
    if (auto* ce = dynamic_cast<const CommentedExpr*>(falseInner)) {
        falseInner = ce->expr.get();
    }
    if (falseInner->kind() == NodeKind::TernaryOp) {
        return fmtTernaryChain(expr, indent, w);
    }
    std::string pad2(indent + w, ' ');
    return expr.condition->toString() + "\n" + pad2 + "? " + fmtBranch(*expr.trueExpr, indent, w) + "\n" + pad2 + ": " +
           fmtBranch(*expr.falseExpr, indent, w);
}

std::string fmtCommentedExpr(const CommentedExpr& expr, int indent, int w) {
    std::string innerPad(indent, ' ');
    int lastLineCommentIdx = -1;
    for (size_t i = 0; i < expr.leadingComments.size(); ++i) {
        if (expr.leadingComments[i]->kind() == NodeKind::CommentLine) {
            lastLineCommentIdx = static_cast<int>(i);
        }
    }
    if (lastLineCommentIdx >= 0) {
        std::vector<std::string> allLines;
        for (int i = 0; i <= lastLineCommentIdx; ++i) {
            allLines.push_back(expr.leadingComments[i]->toString());
        }
        std::string body;
        for (size_t i = static_cast<size_t>(lastLineCommentIdx) + 1; i < expr.leadingComments.size(); ++i) {
            body += expr.leadingComments[i]->toString() + " ";
        }
        body += fmtExpr(*expr.expr, indent, w);
        for (const auto& c : expr.trailingComments) {
            body += " " + c->toString();
        }
        allLines.push_back(body);
        std::string result = allLines[0];
        for (size_t i = 1; i < allLines.size(); ++i) {
            result += "\n" + innerPad + allLines[i];
        }
        return result;
    }
    std::string parts;
    for (const auto& c : expr.leadingComments) {
        parts += c->toString() + " ";
    }
    parts += fmtExpr(*expr.expr, indent, w);
    for (const auto& c : expr.trailingComments) {
        parts += " " + c->toString();
    }
    return parts;
}

std::string fmtLetOpExpr(const LetOp& expr, int indent, int w) {
    std::string pad(indent, ' ');
    std::string innerPad(indent + w, ' ');
    std::vector<std::string> formatted;
    for (const auto& a : expr.assignments) {
        formatted.push_back(fmtAssign(*a, indent + w, w));
    }
    if (formatted.size() > 1 || anyContainsNewline(formatted)) {
        std::string assignLines = join(formatted, ",\n" + innerPad);
        return "let(\n" + innerPad + assignLines + "\n" + pad + ")\n" + pad + fmtExpr(*expr.body, indent, w);
    }
    return "let(" + join(formatted, ", ") + ")\n" + pad + fmtExpr(*expr.body, indent, w);
}

struct SplitLcs {
    std::vector<const ASTNode*> lineComments;
    const ASTNode* cleanedNode = nullptr;
    std::string cleanedOverride;
    bool useOverride = false;
};

SplitLcs splitLcs(const ASTNode& e) {
    if (auto* ce = dynamic_cast<const CommentedExpr*>(&e)) {
        std::vector<const ASTNode*> lcs, rest;
        for (const auto& c : ce->leadingComments) {
            (c->kind() == NodeKind::CommentLine ? lcs : rest).push_back(c.get());
        }
        if (!lcs.empty()) {
            if (rest.empty() && ce->trailingComments.empty()) {
                return SplitLcs{lcs, ce->expr.get(), "", false};
            }
            std::string parts;
            for (auto* c : rest) {
                parts += c->toString() + " ";
            }
            parts += ce->expr->toString();
            for (const auto& c : ce->trailingComments) {
                parts += " " + c->toString();
            }
            return SplitLcs{lcs, nullptr, parts, true};
        }
    }
    return SplitLcs{{}, &e, "", false};
}

std::string fmtListComprehension(const ListComprehension& expr, int indent, int w) {
    std::string pad(indent, ' ');
    std::string innerPad(indent + w, ' ');
    std::vector<SplitLcs> splits;
    for (const auto& e : expr.elements) {
        splits.push_back(splitLcs(*e));
    }
    bool hasLineComment = false;
    for (const auto& s : splits) {
        if (!s.lineComments.empty()) {
            hasLineComment = true;
            break;
        }
    }
    std::vector<std::string> formatted;
    for (const auto& s : splits) {
        formatted.push_back(s.useOverride ? s.cleanedOverride : fmtListElem(*s.cleanedNode, indent + w, w));
    }
    bool anyMultiline = hasLineComment || anyContainsNewline(formatted) || anyEndsWithLineComment(formatted);
    if (!anyMultiline) {
        std::string inlineStr = "[" + join(formatted, ", ") + "]";
        if (static_cast<int>(inlineStr.size()) + indent <= kMultilineCharLimit) {
            return inlineStr;
        }
    }
    std::vector<std::string> lines;
    for (size_t i = 0; i < splits.size(); ++i) {
        std::string comma = (i + 1 == splits.size()) ? "" : ",";
        if (!splits[i].lineComments.empty()) {
            std::string commentStr = "  ";
            for (size_t j = 0; j < splits[i].lineComments.size(); ++j) {
                if (j != 0) {
                    commentStr += "  ";
                }
                commentStr += splits[i].lineComments[j]->toString();
            }
            if (!lines.empty()) {
                lines.back() += commentStr;
            } else {
                for (auto* c : splits[i].lineComments) {
                    lines.push_back(innerPad + c->toString());
                }
            }
        }
        lines.push_back(innerPad + appendTerminatorSafely(formatted[i], comma));
    }
    return "[\n" + join(lines, "\n") + "\n" + pad + "]";
}

std::string fmtExpr(const ASTNode& exprNode, int indent, int w) {
    std::string pad(indent, ' ');
    if (auto* ce = dynamic_cast<const CommentedExpr*>(&exprNode)) {
        return fmtCommentedExpr(*ce, indent, w);
    }
    if (auto* t = dynamic_cast<const TernaryOp*>(&exprNode)) {
        return fmtTernary(*t, indent, w);
    }
    if (auto* a = dynamic_cast<const AssertOp*>(&exprNode)) {
        std::string args = joinToString(a->arguments, ", ");
        if (a->body->kind() == NodeKind::UndefinedLiteral) {
            return "assert(" + args + ")";
        }
        return "assert(" + args + ")\n" + pad + fmtExpr(*a->body, indent, w);
    }
    if (auto* e = dynamic_cast<const EchoOp*>(&exprNode)) {
        std::string args = joinToString(e->arguments, ", ");
        if (e->body->kind() == NodeKind::UndefinedLiteral) {
            return "echo(" + args + ")";
        }
        return "echo(" + args + ")\n" + pad + fmtExpr(*e->body, indent, w);
    }
    if (auto* l = dynamic_cast<const LetOp*>(&exprNode)) {
        return fmtLetOpExpr(*l, indent, w);
    }
    if (auto* call = dynamic_cast<const PrimaryCall*>(&exprNode)) {
        std::string inlineStr = call->toString();
        if (static_cast<int>(inlineStr.size()) + indent > kMultilineCharLimit || endsWithLineComment(inlineStr)) {
            std::vector<std::string> argStrs;
            for (const auto& a : call->arguments) {
                argStrs.push_back(fmtArgument(*a, indent + w, w));
            }
            return fmtMultilineArgsGeneric(call->left->toString(), argStrs, indent, w);
        }
    }
    if (auto bin = asBinaryOp(exprNode)) {
        std::string leftFmt = fmtExpr(*bin->left, indent, w);
        if (startsWith(leftFmt, "[\n")) {
            std::string rightFmt = fmtExpr(*bin->right, indent, w);
            return leftFmt + " " + bin->symbol + " " + rightFmt;
        }
    }
    if (auto* lc = dynamic_cast<const ListComprehension*>(&exprNode)) {
        return fmtListComprehension(*lc, indent, w);
    }
    return exprNode.toString();
}

// A `//` CommentLine runs to end-of-line, so nothing (a statement's `;`, a
// call's closing `)`, ...) may follow it on the same output line without
// being silently swallowed into the comment on re-parse. If `expr` is a
// CommentedExpr with a trailing CommentLine, this splits the formatting
// into (valuePart, afterTerminatorPart) so a caller emitting a statement
// terminator can place it BEFORE the line comment instead of after.
// Otherwise just returns (fmtExpr(expr), "").
std::pair<std::string, std::string> fmtValueBeforeTerminator(const Expression& expr, int indent, int w) {
    if (auto* ce = dynamic_cast<const CommentedExpr*>(&expr)) {
        bool hasLineComment = false;
        for (const auto& c : ce->trailingComments) {
            if (c->kind() == NodeKind::CommentLine) {
                hasLineComment = true;
                break;
            }
        }
        if (hasLineComment) {
            std::string leadingPart;
            for (const auto& c : ce->leadingComments) {
                leadingPart += c->toString() + " ";
            }
            std::string valueStr = leadingPart + fmtExpr(*ce->expr, indent, w);
            std::string trailingPart;
            for (const auto& c : ce->trailingComments) {
                trailingPart += " " + c->toString();
            }
            return {valueStr, trailingPart};
        }
    }
    return {fmtExpr(expr, indent, w), ""};
}

std::string fmtListElem(const ASTNode& elem, int indent, int w) {
    std::string pad(indent, ' ');
    std::string innerPad(indent + w, ' ');
    switch (elem.kind()) {
        case NodeKind::ListCompFor: {
            auto& e = static_cast<const ListCompFor&>(elem);
            std::vector<std::string> formatted;
            for (const auto& a : e.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string body = fmtListElem(*e.body, indent + w, w);
            std::string assignsInline = join(formatted, ", ");
            if (anyContainsNewline(formatted) ||
                static_cast<int>(std::string("for (").size() + assignsInline.size() + 1) + indent > kMultilineCharLimit) {
                return "for (\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")\n" + innerPad + body;
            }
            return "for (" + assignsInline + ")\n" + innerPad + body;
        }
        case NodeKind::ListCompCFor: {
            auto& e = static_cast<const ListCompCFor&>(elem);
            std::vector<std::string> fmtInits, fmtIncrs;
            for (const auto& a : e.inits) {
                fmtInits.push_back(fmtAssign(*a, indent + w, w));
            }
            for (const auto& a : e.incrs) {
                fmtIncrs.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string initsStr = join(fmtInits, ", ");
            std::string incrsStr = join(fmtIncrs, ", ");
            std::string condStr = e.condition->toString();
            std::string body = fmtListElem(*e.body, indent + w, w);
            std::string header = "for (" + initsStr + "; " + condStr + "; " + incrsStr + ")";
            bool anyMultiline = anyContainsNewline(fmtInits) || anyContainsNewline(fmtIncrs) ||
                                 condStr.find('\n') != std::string::npos;
            if (anyMultiline || static_cast<int>(header.size()) + indent > kMultilineCharLimit) {
                return "for (\n" + innerPad + initsStr + ";\n" + innerPad + condStr + ";\n" + innerPad + incrsStr + "\n" +
                       pad + ")\n" + innerPad + body;
            }
            return header + "\n" + innerPad + body;
        }
        case NodeKind::ListCompLet: {
            auto& e = static_cast<const ListCompLet&>(elem);
            std::vector<std::string> formatted;
            for (const auto& a : e.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string body = fmtListElem(*e.body, indent, w);
            if (formatted.size() > 1 || anyContainsNewline(formatted)) {
                return "let(\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")\n" + pad + body;
            }
            return "let(" + join(formatted, ", ") + ")\n" + pad + body;
        }
        case NodeKind::LetOp: {
            auto& e = static_cast<const LetOp&>(elem);
            std::vector<std::string> formatted;
            for (const auto& a : e.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string body = fmtExpr(*e.body, indent, w);
            if (formatted.size() > 1 || anyContainsNewline(formatted)) {
                return "let(\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")\n" + pad + body;
            }
            std::string assigns = join(formatted, ", ");
            std::string inlineStr = "let(" + assigns + ") " + body;
            if (body.find('\n') != std::string::npos || static_cast<int>(inlineStr.size()) + indent > kMultilineCharLimit) {
                return "let(" + assigns + ")\n" + pad + body;
            }
            return inlineStr;
        }
        case NodeKind::ListComprehension:
            return fmtExpr(elem, indent, w);
        case NodeKind::ListCompIf: {
            auto& e = static_cast<const ListCompIf&>(elem);
            std::string body = fmtListElem(*e.trueExpr, indent + w, w);
            return "if (" + e.condition->toString() + ")\n" + innerPad + body;
        }
        case NodeKind::ListCompIfElse: {
            auto& e = static_cast<const ListCompIfElse&>(elem);
            std::string trueBody = fmtListElem(*e.trueExpr, indent + w, w);
            std::string falseBody = fmtListElem(*e.falseExpr, indent + w, w);
            return "if (" + e.condition->toString() + ")\n" + innerPad + trueBody + "\n" + pad + "else\n" + innerPad +
                   falseBody;
        }
        case NodeKind::ListCompEach: {
            auto& e = static_cast<const ListCompEach&>(elem);
            return "each " + fmtListElem(*e.body, indent, w);
        }
        default:
            return elem.toString();
    }
}

std::string fmtChild(const std::vector<std::unique_ptr<ASTNode>>& nodes, int indent, int w) {
    std::string pad(indent, ' ');
    if (nodes.empty()) {
        return ";";
    }
    if (nodes.size() == 1) {
        return "\n" + fmtInst(*nodes[0], indent + w, w, "");
    }
    std::vector<std::string> inner;
    for (const auto& n : nodes) {
        inner.push_back(fmtInst(*n, indent + w, w, ""));
    }
    return " {\n" + join(inner, "\n") + "\n" + pad + "}";
}

std::string fmtBlock(const std::vector<std::unique_ptr<ASTNode>>& nodes, int indent, int w) {
    std::string pad(indent, ' ');
    if (nodes.empty()) {
        return "{}";
    }
    std::vector<std::string> inner;
    for (const auto& n : nodes) {
        inner.push_back(fmtNode(*n, indent + w, w));
    }
    return "{\n" + join(inner, "\n") + "\n" + pad + "}";
}

std::string fmtInst(const ASTNode& node, int indent, int w, const std::string& prefix) {
    std::string pad(indent, ' ');
    if (node.kind() == NodeKind::Assignment) {
        return fmtNode(node, indent, w);
    }
    switch (node.kind()) {
        // Appending (not prepending) preserves outer-to-inner source order
        // when modifiers stack, e.g. `#!cube(1);` (Highlight wrapping
        // ShowOnly) must print back out as "#!", not "!#".
        case NodeKind::ModularModifierShowOnly:
            return fmtInst(*static_cast<const ModularModifierShowOnly&>(node).child, indent, w, prefix + "!");
        case NodeKind::ModularModifierHighlight:
            return fmtInst(*static_cast<const ModularModifierHighlight&>(node).child, indent, w, prefix + "#");
        case NodeKind::ModularModifierBackground:
            return fmtInst(*static_cast<const ModularModifierBackground&>(node).child, indent, w, prefix + "%");
        case NodeKind::ModularModifierDisable:
            return fmtInst(*static_cast<const ModularModifierDisable&>(node).child, indent, w, prefix + "*");

        case NodeKind::ModularCall: {
            auto& n = static_cast<const ModularCall&>(node);
            std::string head = pad + prefix + n.name->name;
            std::string inlineStr = head + "(" + joinToString(n.arguments, ", ") + ")";
            std::string call;
            if (static_cast<int>(inlineStr.size()) > kMultilineCharLimit || endsWithLineComment(inlineStr)) {
                std::vector<std::string> argStrs;
                for (const auto& a : n.arguments) {
                    argStrs.push_back(fmtArgument(*a, indent + w, w));
                }
                call = fmtMultilineArgsGeneric(head, argStrs, indent, w);
            } else {
                call = inlineStr;
            }
            return call + fmtChild(n.children, indent, w);
        }

        case NodeKind::ModularFor: {
            auto& n = static_cast<const ModularFor&>(node);
            std::string innerPad(indent + w, ' ');
            std::vector<std::string> formatted;
            for (const auto& a : n.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string inlineStr = pad + prefix + "for (" + join(formatted, ", ") + ")";
            std::string head;
            if (static_cast<int>(inlineStr.size()) > kMultilineCharLimit || anyContainsNewline(formatted) ||
                anyEndsWithLineComment(formatted)) {
                head = pad + prefix + "for (\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")";
            } else {
                head = inlineStr;
            }
            return head + fmtChild(n.body, indent, w);
        }

        case NodeKind::ModularIntersectionFor: {
            auto& n = static_cast<const ModularIntersectionFor&>(node);
            std::string innerPad(indent + w, ' ');
            std::vector<std::string> formatted;
            for (const auto& a : n.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            std::string inlineStr = pad + prefix + "intersection_for (" + join(formatted, ", ") + ")";
            std::string head;
            if (static_cast<int>(inlineStr.size()) > kMultilineCharLimit || anyContainsNewline(formatted) ||
                anyEndsWithLineComment(formatted)) {
                head =
                    pad + prefix + "intersection_for (\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")";
            } else {
                head = inlineStr;
            }
            return head + fmtChild(n.body, indent, w);
        }

        case NodeKind::ModularLet: {
            auto& n = static_cast<const ModularLet&>(node);
            std::string innerPad(indent + w, ' ');
            std::vector<std::string> formatted;
            for (const auto& a : n.assignments) {
                formatted.push_back(fmtAssign(*a, indent + w, w));
            }
            if (formatted.size() > 1 || anyContainsNewline(formatted) || anyEndsWithLineComment(formatted)) {
                std::string tail = fmtChild(n.children, indent, w);
                return pad + prefix + "let (\n" + innerPad + join(formatted, ",\n" + innerPad) + "\n" + pad + ")" + tail;
            }
            return pad + prefix + "let (" + join(formatted, ", ") + ")" + fmtChild(n.children, indent, w);
        }

        case NodeKind::ModularEcho: {
            auto& n = static_cast<const ModularEcho&>(node);
            std::string head = pad + prefix + "echo";
            std::string inlineStr = head + "(" + joinToString(n.arguments, ", ") + ")";
            std::string call;
            if (static_cast<int>(inlineStr.size()) > kMultilineCharLimit || endsWithLineComment(inlineStr)) {
                std::vector<std::string> argStrs;
                for (const auto& a : n.arguments) {
                    argStrs.push_back(fmtArgument(*a, indent + w, w));
                }
                call = fmtMultilineArgsGeneric(head, argStrs, indent, w);
            } else {
                call = inlineStr;
            }
            return call + fmtChild(n.children, indent, w);
        }

        case NodeKind::ModularAssert: {
            auto& n = static_cast<const ModularAssert&>(node);
            std::string head = pad + prefix + "assert";
            std::string inlineStr = head + "(" + joinToString(n.arguments, ", ") + ")";
            std::string call;
            if (static_cast<int>(inlineStr.size()) > kMultilineCharLimit || endsWithLineComment(inlineStr)) {
                std::vector<std::string> argStrs;
                for (const auto& a : n.arguments) {
                    argStrs.push_back(fmtArgument(*a, indent + w, w));
                }
                call = fmtMultilineArgsGeneric(head, argStrs, indent, w);
            } else {
                call = inlineStr;
            }
            return call + fmtChild(n.children, indent, w);
        }

        case NodeKind::ModularIf: {
            auto& n = static_cast<const ModularIf&>(node);
            std::string header = pad + prefix + "if (" + n.condition->toString() + ")";
            return header + fmtChild(n.trueBranch, indent, w);
        }

        case NodeKind::ModularIfElse: {
            auto& n = static_cast<const ModularIfElse&>(node);
            std::string header = pad + prefix + "if (" + n.condition->toString() + ")";
            std::string trueTail = fmtChild(n.trueBranch, indent, w);
            std::string falseTail = fmtChild(n.falseBranch, indent, w);
            std::string connector = startsWith(trueTail, " {") ? " else" : ("\n" + pad + "else");
            return header + trueTail + connector + falseTail;
        }

        default:
            return pad + prefix + node.toString() + ";";
    }
}

std::string fmtNode(const ASTNode& node, int indent, int w) {
    std::string pad(indent, ' ');
    switch (node.kind()) {
        case NodeKind::BlankLine:
            return "";
        case NodeKind::CommentLine:
            return pad + "//" + static_cast<const CommentLine&>(node).text;
        case NodeKind::CommentSpan:
            return pad + "/*" + static_cast<const CommentSpan&>(node).text + "*/";
        case NodeKind::UseStatement: {
            auto& u = static_cast<const UseStatement&>(node);
            return pad + "use <" + u.filepath->val + ">";
        }
        case NodeKind::IncludeStatement: {
            auto& u = static_cast<const IncludeStatement&>(node);
            return pad + "include <" + u.filepath->val + ">";
        }
        case NodeKind::Assignment: {
            auto& n = static_cast<const Assignment&>(node);
            auto [rhs, afterSemi] = fmtValueBeforeTerminator(*n.expr, indent, w);
            if (startsWith(rhs, "[\n")) {
                std::tie(rhs, afterSemi) = fmtValueBeforeTerminator(*n.expr, indent + w, w);
                return pad + n.name->name + " = " + rhs + ";" + afterSemi;
            }
            std::string inlineStr = pad + n.name->name + " = " + rhs + ";" + afterSemi;
            std::string firstLine = inlineStr.substr(0, inlineStr.find('\n'));
            if (static_cast<int>(firstLine.size()) > kMultilineCharLimit) {
                std::tie(rhs, afterSemi) = fmtValueBeforeTerminator(*n.expr, indent + w, w);
                return pad + n.name->name + " =\n" + std::string(indent + w, ' ') + rhs + ";" + afterSemi;
            }
            return inlineStr;
        }
        case NodeKind::FunctionDeclaration: {
            auto& n = static_cast<const FunctionDeclaration&>(node);
            std::string pre = joinComments(n.preNameComments);
            std::string postN = joinComments(n.postNameComments);
            std::string postP = joinComments(n.postParamsComments);
            std::string head =
                pad + "function" + (pre.empty() ? "" : " " + pre) + " " + n.name->name + (postN.empty() ? "" : " " + postN);
            std::string paramsInline = joinParams(n.parameters);
            std::string postPStr = postP.empty() ? "" : " " + postP;
            std::string exprPad(indent + w, ' ');
            if (static_cast<int>((head + "(" + paramsInline + ")" + postPStr + " =").size()) > kMultilineCharLimit) {
                std::vector<std::string> paramStrs;
                for (const auto& p : n.parameters) {
                    paramStrs.push_back(fmtParameter(*p));
                }
                std::string paramBlock = fmtMultilineArgsGeneric(head, paramStrs, indent, w);
                auto [rhs, afterSemi] = fmtValueBeforeTerminator(*n.expr, indent + w, w);
                return paramBlock + postPStr + " =\n" + exprPad + rhs + ";" + afterSemi;
            }
            auto [rhs, afterSemi] = fmtValueBeforeTerminator(*n.expr, indent + w, w);
            return head + "(" + paramsInline + ")" + postPStr + " =\n" + exprPad + rhs + ";" + afterSemi;
        }
        case NodeKind::ModuleDeclaration: {
            auto& n = static_cast<const ModuleDeclaration&>(node);
            std::string pre = joinComments(n.preNameComments);
            std::string postN = joinComments(n.postNameComments);
            std::string postP = joinComments(n.postParamsComments);
            std::string head =
                pad + "module" + (pre.empty() ? "" : " " + pre) + " " + n.name->name + (postN.empty() ? "" : " " + postN);
            std::string paramsInline = joinParams(n.parameters);
            std::string postPStr = postP.empty() ? "" : " " + postP;
            std::string block = fmtBlock(n.children, indent, w);
            if (static_cast<int>((head + "(" + paramsInline + ")" + postPStr).size()) > kMultilineCharLimit) {
                std::vector<std::string> paramStrs;
                for (const auto& p : n.parameters) {
                    paramStrs.push_back(fmtParameter(*p));
                }
                std::string paramBlock = fmtMultilineArgsGeneric(head, paramStrs, indent, w);
                return paramBlock + postPStr + " " + block;
            }
            return head + "(" + paramsInline + ")" + postPStr + " " + block;
        }
        default:
            if (isModuleInstantiationKind(node.kind())) {
                return fmtInst(node, indent, w, "");
            }
            return pad + node.toString();
    }
}

std::string coalesceParenBracket(const std::string& text) {
    std::vector<std::string> lines = splitLines(text);
    std::vector<std::string> result;
    size_t i = 0;
    while (i < lines.size()) {
        if (i + 1 < lines.size() && strip(lines[i]) == ")" && startsWith(lstrip(lines[i + 1]), "[")) {
            size_t indentLen = lines[i].size() - lstrip(lines[i]).size();
            result.push_back(std::string(indentLen, ' ') + ") " + lstrip(lines[i + 1]));
            i += 2;
        } else {
            result.push_back(lines[i]);
            i += 1;
        }
    }
    return join(result, "\n");
}

} // namespace

std::string toOpenscad(const std::vector<std::unique_ptr<ASTNode>>& nodes, int indentWidth) {
    std::vector<std::string> parts;
    bool prevComplex = false;
    for (const auto& node : nodes) {
        bool isComplex = node->kind() == NodeKind::ModuleDeclaration || node->kind() == NodeKind::FunctionDeclaration;
        bool isBlank = node->kind() == NodeKind::BlankLine;
        if (!parts.empty() && prevComplex && !isBlank) {
            parts.push_back("");
            parts.push_back("");
        }
        parts.push_back(fmtNode(*node, 0, indentWidth));
        if (!isBlank) {
            prevComplex = isComplex;
        }
    }
    return coalesceParenBracket(join(parts, "\n"));
}

} // namespace oscad
