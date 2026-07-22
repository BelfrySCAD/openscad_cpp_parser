#include "comment_attach_internal.hpp"

#include <algorithm>
#include <functional>

namespace oscad {

namespace {

// A place in the tree currently holding an Expression that's eligible to
// be wrapped in a CommentedExpr. `take`/`put` transfer ownership out of
// and back into whatever the underlying storage actually is
// (unique_ptr<Expression> field, or unique_ptr<ASTNode> field known to
// dynamically hold an Expression) -- both produce/consume
// unique_ptr<Expression>, so callers don't need to care which.
struct ExprSlot {
    Expression* current = nullptr;
    std::function<std::unique_ptr<Expression>()> take;
    std::function<void(std::unique_ptr<Expression>)> put;
};

void addExpr(std::unique_ptr<Expression>& field, std::vector<ExprSlot>& exprOut) {
    if (field && field->kind() != NodeKind::CommentedExpr) {
        exprOut.push_back(ExprSlot{field.get(), [&field] { return std::move(field); },
                                    [&field](std::unique_ptr<Expression> v) { field = std::move(v); }});
    }
}

void addExprFromAstNode(std::unique_ptr<ASTNode>& field, std::vector<ExprSlot>& exprOut, std::vector<ASTNode*>& childOut) {
    if (!field) {
        return;
    }
    if (field->kind() == NodeKind::CommentedExpr) {
        childOut.push_back(field.get());
        return;
    }
    if (dynamic_cast<Expression*>(field.get()) != nullptr) {
        exprOut.push_back(ExprSlot{
            static_cast<Expression*>(field.get()),
            [&field] {
                std::unique_ptr<Expression> result(static_cast<Expression*>(field.release()));
                return result;
            },
            [&field](std::unique_ptr<Expression> v) { field = std::move(v); },
        });
    } else {
        childOut.push_back(field.get());
    }
}

void addChild(ASTNode* node, std::vector<ASTNode*>& childOut) {
    if (node != nullptr) {
        childOut.push_back(node);
    }
}

// Argument/Assignment/ParameterDeclaration are "container" types whose own
// single value-expression field must be unwrapped directly into the
// PARENT's exprFields, not deferred to a later recursive walkAttach call
// into the container itself. This matters because these containers' own
// position span starts at their *value* (e.g. a PositionalArgument's span
// is just its expr's span) -- it does NOT include a comment sitting in the
// gap between the parent's bracket and the value, e.g. `cube(/* n */ 5)`.
// If that gap comment were only checked against the PositionalArgument's
// own (tight) span, it would never match (the comment lies outside it) and
// would silently fail to attach. Unwrapping one level up, so the match
// happens against ModularCall's own (wide) span instead, is exactly what
// fixes this -- mirrors the reference's _collect_container_exprs.
void addArgumentExprList(std::vector<std::unique_ptr<Argument>>& items, std::vector<ExprSlot>& exprOut) {
    for (auto& item : items) {
        if (!item) {
            continue;
        }
        if (auto* pa = dynamic_cast<PositionalArgument*>(item.get())) {
            addExpr(pa->expr, exprOut);
        } else if (auto* na = dynamic_cast<NamedArgument*>(item.get())) {
            addExpr(na->expr, exprOut);
        }
    }
}

void addAssignmentExprList(std::vector<std::unique_ptr<Assignment>>& items, std::vector<ExprSlot>& exprOut) {
    for (auto& item : items) {
        if (item) {
            addExpr(item->expr, exprOut);
        }
    }
}

void addParameterExprList(std::vector<std::unique_ptr<ParameterDeclaration>>& items, std::vector<ExprSlot>& exprOut) {
    for (auto& item : items) {
        if (item && item->defaultValue) {
            addExpr(item->defaultValue, exprOut);
        }
    }
}

void addAstNodeList(std::vector<std::unique_ptr<ASTNode>>& items, std::vector<ExprSlot>& exprOut,
                     std::vector<ASTNode*>& childOut) {
    for (auto& item : items) {
        addExprFromAstNode(item, exprOut, childOut);
    }
}

// Splits `node`'s direct children into value-expression slots (candidates
// for CommentedExpr wrapping) and other ASTNode children (recursed into,
// where their own value-expression fields get discovered in turn).
//
// Argument/Assignment/ParameterDeclaration list items are unwrapped ONE
// LEVEL directly into exprFields here (via addArgumentExprList & friends),
// matching the reference's _collect_container_exprs -- this isn't optional
// polish, it's required for correct matching: a container's own position
// span starts at its *value* (e.g. a PositionalArgument's span is just its
// expr's span), so a comment sitting between the parent's bracket and the
// value -- `cube(/* n */ 5)` -- lies outside the container's own span and
// would never match if left for a later recursive call into the container
// itself. Matching it against the wider PARENT span (ModularCall here)
// instead is what makes it work.
//
// Every OTHER non-Expression ASTNode child (nested module instantiations,
// declarations, etc.) is instead pushed to nonExprChildren for a full
// recursive visit -- unlike the reference, which stops unwrapping a
// container after one level and only adds it to non_expr_children if that
// one level found no expression at all. That's a real gap in the
// reference: a nested module call's `name` becomes a candidate slot via
// the one-level unwrap, so `found_expr` is true and the call is dropped
// entirely rather than recursed into -- its own `arguments`/`children`
// then never get visited when it's a list item elsewhere. Recursing here
// unconditionally sidesteps that.
//
// Declarative-identity Identifier fields (variable/module/function names,
// `.member` in `a.b`) are never treated as wrappable, even though
// Identifier IS-A Expression -- wrapping a name in a comment doesn't make
// sense the way wrapping a value does, and the reference's generic
// reflection can't tell the difference.
void classifyNode(ASTNode& node, std::vector<ExprSlot>& exprFields, std::vector<ASTNode*>& nonExprChildren) {
    switch (node.kind()) {
        case NodeKind::CommentLine:
        case NodeKind::BlankLine:
        case NodeKind::CommentSpan:
        case NodeKind::CommentedExpr: // never classified directly; short-circuited by the caller
        case NodeKind::Identifier:
        case NodeKind::StringLiteral:
        case NodeKind::NumberLiteral:
        case NodeKind::BooleanLiteral:
        case NodeKind::UndefinedLiteral:
        case NodeKind::UseStatement:
        case NodeKind::IncludeStatement:
            break;

        case NodeKind::RangeLiteral: {
            auto& n = static_cast<RangeLiteral&>(node);
            addExpr(n.start, exprFields);
            addExpr(n.end, exprFields);
            addExpr(n.step, exprFields);
            break;
        }
        case NodeKind::ParameterDeclaration:
            addExpr(static_cast<ParameterDeclaration&>(node).defaultValue, exprFields);
            break;
        case NodeKind::PositionalArgument:
            addExpr(static_cast<PositionalArgument&>(node).expr, exprFields);
            break;
        case NodeKind::NamedArgument:
            addExpr(static_cast<NamedArgument&>(node).expr, exprFields);
            break;
        case NodeKind::Assignment:
            addExpr(static_cast<Assignment&>(node).expr, exprFields);
            break;

        case NodeKind::LetOp: {
            auto& n = static_cast<LetOp&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addExpr(n.body, exprFields);
            break;
        }
        case NodeKind::EchoOp: {
            auto& n = static_cast<EchoOp&>(node);
            addArgumentExprList(n.arguments, exprFields);
            addExpr(n.body, exprFields);
            break;
        }
        case NodeKind::AssertOp: {
            auto& n = static_cast<AssertOp&>(node);
            addArgumentExprList(n.arguments, exprFields);
            addExpr(n.body, exprFields);
            break;
        }
        case NodeKind::FunctionLiteral: {
            auto& n = static_cast<FunctionLiteral&>(node);
            addParameterExprList(n.parameters, exprFields);
            addExpr(n.body, exprFields);
            break;
        }

#define OSCAD_CLASSIFY_UNARY(Kind, Type)                                                                              \
    case NodeKind::Kind:                                                                                              \
        addExpr(static_cast<Type&>(node).expr, exprFields);                                                          \
        break;

            OSCAD_CLASSIFY_UNARY(UnaryMinusOp, UnaryMinusOp)
            OSCAD_CLASSIFY_UNARY(LogicalNotOp, LogicalNotOp)
            OSCAD_CLASSIFY_UNARY(BitwiseNotOp, BitwiseNotOp)
#undef OSCAD_CLASSIFY_UNARY

#define OSCAD_CLASSIFY_BINARY(Kind, Type)                                                                              \
    case NodeKind::Kind: {                                                                                             \
        auto& n = static_cast<Type&>(node);                                                                           \
        addExpr(n.left, exprFields);                                                                                  \
        addExpr(n.right, exprFields);                                                                                 \
        break;                                                                                                         \
    }

            OSCAD_CLASSIFY_BINARY(AdditionOp, AdditionOp)
            OSCAD_CLASSIFY_BINARY(SubtractionOp, SubtractionOp)
            OSCAD_CLASSIFY_BINARY(MultiplicationOp, MultiplicationOp)
            OSCAD_CLASSIFY_BINARY(DivisionOp, DivisionOp)
            OSCAD_CLASSIFY_BINARY(ModuloOp, ModuloOp)
            OSCAD_CLASSIFY_BINARY(ExponentOp, ExponentOp)
            OSCAD_CLASSIFY_BINARY(BitwiseAndOp, BitwiseAndOp)
            OSCAD_CLASSIFY_BINARY(BitwiseOrOp, BitwiseOrOp)
            OSCAD_CLASSIFY_BINARY(BitwiseShiftLeftOp, BitwiseShiftLeftOp)
            OSCAD_CLASSIFY_BINARY(BitwiseShiftRightOp, BitwiseShiftRightOp)
            OSCAD_CLASSIFY_BINARY(LogicalAndOp, LogicalAndOp)
            OSCAD_CLASSIFY_BINARY(LogicalOrOp, LogicalOrOp)
            OSCAD_CLASSIFY_BINARY(EqualityOp, EqualityOp)
            OSCAD_CLASSIFY_BINARY(InequalityOp, InequalityOp)
            OSCAD_CLASSIFY_BINARY(GreaterThanOp, GreaterThanOp)
            OSCAD_CLASSIFY_BINARY(GreaterThanOrEqualOp, GreaterThanOrEqualOp)
            OSCAD_CLASSIFY_BINARY(LessThanOp, LessThanOp)
            OSCAD_CLASSIFY_BINARY(LessThanOrEqualOp, LessThanOrEqualOp)
#undef OSCAD_CLASSIFY_BINARY

        case NodeKind::TernaryOp: {
            auto& n = static_cast<TernaryOp&>(node);
            addExpr(n.condition, exprFields);
            addExpr(n.trueExpr, exprFields);
            addExpr(n.falseExpr, exprFields);
            break;
        }

        case NodeKind::PrimaryCall: {
            auto& n = static_cast<PrimaryCall&>(node);
            addExpr(n.left, exprFields);
            addArgumentExprList(n.arguments, exprFields);
            break;
        }
        case NodeKind::PrimaryIndex: {
            auto& n = static_cast<PrimaryIndex&>(node);
            addExpr(n.left, exprFields);
            addExpr(n.index, exprFields);
            break;
        }
        case NodeKind::PrimaryMember:
            addExpr(static_cast<PrimaryMember&>(node).left, exprFields);
            break;

        case NodeKind::ListCompLet: {
            auto& n = static_cast<ListCompLet&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addExprFromAstNode(n.body, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ListCompEach:
            addExprFromAstNode(static_cast<ListCompEach&>(node).body, exprFields, nonExprChildren);
            break;
        case NodeKind::ListCompFor: {
            auto& n = static_cast<ListCompFor&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addExprFromAstNode(n.body, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ListCompCFor: {
            auto& n = static_cast<ListCompCFor&>(node);
            addAssignmentExprList(n.inits, exprFields);
            addExpr(n.condition, exprFields);
            addAssignmentExprList(n.incrs, exprFields);
            addExprFromAstNode(n.body, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ListCompIf: {
            auto& n = static_cast<ListCompIf&>(node);
            addExpr(n.condition, exprFields);
            addExprFromAstNode(n.trueExpr, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ListCompIfElse: {
            auto& n = static_cast<ListCompIfElse&>(node);
            addExpr(n.condition, exprFields);
            addExprFromAstNode(n.trueExpr, exprFields, nonExprChildren);
            addExprFromAstNode(n.falseExpr, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ListComprehension:
            addAstNodeList(static_cast<ListComprehension&>(node).elements, exprFields, nonExprChildren);
            break;

        case NodeKind::ModularCall: {
            auto& n = static_cast<ModularCall&>(node);
            addArgumentExprList(n.arguments, exprFields);
            addAstNodeList(n.children, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularFor: {
            auto& n = static_cast<ModularFor&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addAstNodeList(n.body, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularIntersectionFor: {
            auto& n = static_cast<ModularIntersectionFor&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addAstNodeList(n.body, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularLet: {
            auto& n = static_cast<ModularLet&>(node);
            addAssignmentExprList(n.assignments, exprFields);
            addAstNodeList(n.children, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularEcho: {
            auto& n = static_cast<ModularEcho&>(node);
            addArgumentExprList(n.arguments, exprFields);
            addAstNodeList(n.children, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularAssert: {
            auto& n = static_cast<ModularAssert&>(node);
            addArgumentExprList(n.arguments, exprFields);
            addAstNodeList(n.children, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularIf: {
            auto& n = static_cast<ModularIf&>(node);
            addExpr(n.condition, exprFields);
            addAstNodeList(n.trueBranch, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::ModularIfElse: {
            auto& n = static_cast<ModularIfElse&>(node);
            addExpr(n.condition, exprFields);
            addAstNodeList(n.trueBranch, exprFields, nonExprChildren);
            addAstNodeList(n.falseBranch, exprFields, nonExprChildren);
            break;
        }

#define OSCAD_CLASSIFY_MODIFIER(Kind, Type)                                                                            \
    case NodeKind::Kind:                                                                                               \
        addChild(static_cast<Type&>(node).child.get(), nonExprChildren);                                              \
        break;

            OSCAD_CLASSIFY_MODIFIER(ModularModifierShowOnly, ModularModifierShowOnly)
            OSCAD_CLASSIFY_MODIFIER(ModularModifierHighlight, ModularModifierHighlight)
            OSCAD_CLASSIFY_MODIFIER(ModularModifierBackground, ModularModifierBackground)
            OSCAD_CLASSIFY_MODIFIER(ModularModifierDisable, ModularModifierDisable)
#undef OSCAD_CLASSIFY_MODIFIER

        case NodeKind::ModuleDeclaration: {
            auto& n = static_cast<ModuleDeclaration&>(node);
            addParameterExprList(n.parameters, exprFields);
            addAstNodeList(n.children, exprFields, nonExprChildren);
            break;
        }
        case NodeKind::FunctionDeclaration: {
            auto& n = static_cast<FunctionDeclaration&>(node);
            addParameterExprList(n.parameters, exprFields);
            addExpr(n.expr, exprFields);
            break;
        }
    }
}

int startOffsetOf(const ASTNode& n) { return n.position().start_offset; }
int endOffsetOf(const ASTNode& n) { return n.position().end_offset; }

// Recursively attaches unused entries of `comments` (sorted by
// start_offset; a used entry is left null) to expressions within `node`'s
// span. Mirrors _walk_attach.
void walkAttach(ASTNode& node, std::vector<std::unique_ptr<ASTNode>>& comments) {
    if (node.kind() == NodeKind::CommentedExpr || node.kind() == NodeKind::CommentLine ||
        node.kind() == NodeKind::CommentSpan) {
        return;
    }

    int ns = startOffsetOf(node);
    int ne = endOffsetOf(node);

    bool hasRelevant = false;
    for (const auto& c : comments) {
        if (!c) {
            continue;
        }
        int cs = startOffsetOf(*c);
        if (ns <= cs && cs < ne) {
            hasRelevant = true;
            break;
        }
    }

    std::vector<ExprSlot> exprFields;
    std::vector<ASTNode*> nonExprChildren;
    classifyNode(node, exprFields, nonExprChildren);

    if (!hasRelevant) {
        for (auto* child : nonExprChildren) {
            walkAttach(*child, comments);
        }
        for (auto& slot : exprFields) {
            walkAttach(*slot.current, comments);
        }
        return;
    }

    std::sort(exprFields.begin(), exprFields.end(),
              [](const ExprSlot& a, const ExprSlot& b) { return startOffsetOf(*a.current) < startOffsetOf(*b.current); });

    std::vector<std::vector<size_t>> leadingIdx(exprFields.size());
    std::vector<std::vector<size_t>> trailingIdx(exprFields.size());

    for (size_t ci = 0; ci < comments.size(); ++ci) {
        if (!comments[ci]) {
            continue;
        }
        int cs = startOffsetOf(*comments[ci]);
        if (cs < ns || cs >= ne) {
            continue;
        }
        bool attached = false;
        for (size_t ei = 0; ei < exprFields.size(); ++ei) {
            int es = startOffsetOf(*exprFields[ei].current);
            if (cs < es) {
                leadingIdx[ei].push_back(ci);
                attached = true;
                break;
            }
            // Otherwise the comment is inside or past this field's span --
            // leave it for a deeper recursive call (into that field, or
            // whichever one actually contains it) to pick up, exactly as
            // in the reference.
        }
        if (!attached && !exprFields.empty()) {
            size_t lastEi = exprFields.size() - 1;
            if (cs >= endOffsetOf(*exprFields[lastEi].current)) {
                trailingIdx[lastEi].push_back(ci);
            }
        }
    }

    for (size_t ei = 0; ei < exprFields.size(); ++ei) {
        if (leadingIdx[ei].empty() && trailingIdx[ei].empty()) {
            continue;
        }
        std::vector<std::unique_ptr<ASTNode>> leadingComments;
        for (size_t ci : leadingIdx[ei]) {
            leadingComments.push_back(std::move(comments[ci]));
        }
        std::vector<std::unique_ptr<ASTNode>> trailingComments;
        for (size_t ci : trailingIdx[ei]) {
            trailingComments.push_back(std::move(comments[ci]));
        }

        std::unique_ptr<Expression> original = exprFields[ei].take();
        Position pos = original->position();
        auto wrapped =
            std::make_unique<CommentedExpr>(pos, std::move(leadingComments), std::move(trailingComments), std::move(original));
        exprFields[ei].put(std::move(wrapped));
    }

    for (auto& slot : exprFields) {
        walkAttach(*slot.current, comments);
    }
    for (auto* child : nonExprChildren) {
        walkAttach(*child, comments);
    }
}

// Mirrors _attach_trailing_to_last_expr: wraps the LAST value-expression
// slot found in `node` (in field-declaration order, not sorted by
// position -- matching the reference) with `comment` appended as trailing.
// No-op if `node` has no wrappable expression at all.
void attachTrailingToLastExpr(ASTNode& node, std::unique_ptr<ASTNode> comment) {
    std::vector<ExprSlot> exprFields;
    std::vector<ASTNode*> nonExprChildren;
    classifyNode(node, exprFields, nonExprChildren);
    if (exprFields.empty()) {
        return;
    }
    ExprSlot& slot = exprFields.back();
    std::unique_ptr<Expression> original = slot.take();
    Position pos = original->position();
    std::vector<std::unique_ptr<ASTNode>> trailingComments;
    trailingComments.push_back(std::move(comment));
    auto wrapped = std::make_unique<CommentedExpr>(pos, std::vector<std::unique_ptr<ASTNode>>{}, std::move(trailingComments),
                                                    std::move(original));
    slot.put(std::move(wrapped));
}

} // namespace

void attachInlineComments(std::vector<std::unique_ptr<ASTNode>>& astNodes,
                           std::vector<std::unique_ptr<ASTNode>> inlineComments) {
    if (inlineComments.empty()) {
        return;
    }
    std::sort(inlineComments.begin(), inlineComments.end(),
              [](const auto& a, const auto& b) { return startOffsetOf(*a) < startOffsetOf(*b); });

    for (auto& node : astNodes) {
        if (node) {
            walkAttach(*node, inlineComments);
        }
    }

    // Fallback: any comment still unused falls within some node's span but
    // didn't line up with a specific expression slot there (or the
    // top-level list has no matching span at all, e.g. a comment after the
    // last statement) -- attach it as trailing on the nearest PRECEDING
    // top-level node's last expression field.
    for (auto& comment : inlineComments) {
        if (!comment) {
            continue;
        }
        int cs = startOffsetOf(*comment);
        ASTNode* bestNode = nullptr;
        for (auto& node : astNodes) {
            if (!node) {
                continue;
            }
            NodeKind k = node->kind();
            if (k == NodeKind::CommentLine || k == NodeKind::CommentSpan || k == NodeKind::BlankLine) {
                continue;
            }
            if (endOffsetOf(*node) <= cs) {
                bestNode = node.get();
            }
        }
        if (bestNode != nullptr) {
            attachTrailingToLastExpr(*bestNode, std::move(comment));
        }
        // else: no preceding node to attach to -- comment is dropped.
    }
}

} // namespace oscad
