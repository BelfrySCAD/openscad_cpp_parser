#include "openscad_cpp_parser/serialization.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace oscad {

using json = nlohmann::json;

namespace {

template <typename Derived, typename Base>
std::unique_ptr<Derived> cast(std::unique_ptr<Base> node) {
    return std::unique_ptr<Derived>(static_cast<Derived*>(node.release()));
}

json positionToJson(const Position& pos) {
    return json{{"origin", pos.origin}, {"line", pos.line}, {"column", pos.column},
                {"start_offset", pos.start_offset}, {"end_offset", pos.end_offset}};
}

Position positionFromJson(const json& j) {
    if (!j.contains("_position") || j["_position"].is_null()) {
        return Position{"<unknown>", 0, 0, 0, 0};
    }
    const auto& p = j["_position"];
    return Position{p.at("origin").get<std::string>(), p.at("line").get<int>(), p.at("column").get<int>(),
                     p.value("start_offset", 0), p.value("end_offset", 0)};
}

json valueToJson(const ASTNode* node, bool includePos) {
    if (!node) {
        return nullptr;
    }
    return astToJson(*node, includePos);
}

template <typename T>
json listToJson(const std::vector<std::unique_ptr<T>>& items, bool includePos) {
    json arr = json::array();
    for (const auto& item : items) {
        arr.push_back(astToJson(*item, includePos));
    }
    return arr;
}

template <typename T>
std::unique_ptr<T> childFromJson(const json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) {
        return nullptr;
    }
    return cast<T>(astFromJson(j.at(key)));
}

template <typename T>
std::vector<std::unique_ptr<T>> listFromJson(const json& j, const char* key) {
    std::vector<std::unique_ptr<T>> result;
    if (j.contains(key)) {
        for (const auto& item : j.at(key)) {
            result.push_back(cast<T>(astFromJson(item)));
        }
    }
    return result;
}

using Builder = std::function<std::unique_ptr<ASTNode>(const json&, Position)>;

// -- toJson: one case per instantiable NodeKind ----------------------------

json toJsonImpl(const ASTNode& node, bool includePos) {
    json j;
    j["_type"] = nodeKindName(node.kind());
    if (includePos) {
        j["_position"] = positionToJson(node.position());
    }

    switch (node.kind()) {
        case NodeKind::CommentLine:
            j["text"] = static_cast<const CommentLine&>(node).text;
            break;
        case NodeKind::BlankLine:
            break;
        case NodeKind::CommentSpan:
            j["text"] = static_cast<const CommentSpan&>(node).text;
            break;
        case NodeKind::CommentedExpr: {
            auto& n = static_cast<const CommentedExpr&>(node);
            j["leading_comments"] = listToJson(n.leadingComments, includePos);
            j["trailing_comments"] = listToJson(n.trailingComments, includePos);
            j["expr"] = valueToJson(n.expr.get(), includePos);
            break;
        }
        case NodeKind::Identifier:
            j["name"] = static_cast<const Identifier&>(node).name;
            break;
        case NodeKind::StringLiteral:
            j["val"] = static_cast<const StringLiteral&>(node).val;
            break;
        case NodeKind::NumberLiteral:
            j["val"] = static_cast<const NumberLiteral&>(node).val;
            break;
        case NodeKind::BooleanLiteral:
            j["val"] = static_cast<const BooleanLiteral&>(node).val;
            break;
        case NodeKind::UndefinedLiteral:
            break;
        case NodeKind::RangeLiteral: {
            auto& n = static_cast<const RangeLiteral&>(node);
            j["start"] = valueToJson(n.start.get(), includePos);
            j["end"] = valueToJson(n.end.get(), includePos);
            j["step"] = valueToJson(n.step.get(), includePos);
            if (n.implicitStep) j["implicitStep"] = true;
            break;
        }
        case NodeKind::ParameterDeclaration: {
            auto& n = static_cast<const ParameterDeclaration&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["default"] = valueToJson(n.defaultValue.get(), includePos);
            j["leading_comments"] = listToJson(n.leadingComments, includePos);
            j["trailing_comments"] = listToJson(n.trailingComments, includePos);
            break;
        }
        case NodeKind::PositionalArgument:
            j["expr"] = valueToJson(static_cast<const PositionalArgument&>(node).expr.get(), includePos);
            break;
        case NodeKind::NamedArgument: {
            auto& n = static_cast<const NamedArgument&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["expr"] = valueToJson(n.expr.get(), includePos);
            break;
        }
        case NodeKind::Assignment: {
            auto& n = static_cast<const Assignment&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["expr"] = valueToJson(n.expr.get(), includePos);
            break;
        }
        case NodeKind::LetOp: {
            auto& n = static_cast<const LetOp&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::EchoOp: {
            auto& n = static_cast<const EchoOp&>(node);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::AssertOp: {
            auto& n = static_cast<const AssertOp&>(node);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::FunctionLiteral: {
            auto& n = static_cast<const FunctionLiteral&>(node);
            j["parameters"] = listToJson(n.parameters, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }

#define OSCAD_JSON_UNARY(Kind, Type)                                                                                  \
    case NodeKind::Kind:                                                                                              \
        j["expr"] = valueToJson(static_cast<const Type&>(node).expr.get(), includePos);                              \
        break;

            OSCAD_JSON_UNARY(UnaryMinusOp, UnaryMinusOp)
            OSCAD_JSON_UNARY(LogicalNotOp, LogicalNotOp)
            OSCAD_JSON_UNARY(BitwiseNotOp, BitwiseNotOp)
#undef OSCAD_JSON_UNARY

#define OSCAD_JSON_BINARY(Kind, Type)                                                                                  \
    case NodeKind::Kind: {                                                                                             \
        auto& n = static_cast<const Type&>(node);                                                                      \
        j["left"] = valueToJson(n.left.get(), includePos);                                                             \
        j["right"] = valueToJson(n.right.get(), includePos);                                                           \
        break;                                                                                                          \
    }

            OSCAD_JSON_BINARY(AdditionOp, AdditionOp)
            OSCAD_JSON_BINARY(SubtractionOp, SubtractionOp)
            OSCAD_JSON_BINARY(MultiplicationOp, MultiplicationOp)
            OSCAD_JSON_BINARY(DivisionOp, DivisionOp)
            OSCAD_JSON_BINARY(ModuloOp, ModuloOp)
            OSCAD_JSON_BINARY(ExponentOp, ExponentOp)
            OSCAD_JSON_BINARY(BitwiseAndOp, BitwiseAndOp)
            OSCAD_JSON_BINARY(BitwiseOrOp, BitwiseOrOp)
            OSCAD_JSON_BINARY(BitwiseShiftLeftOp, BitwiseShiftLeftOp)
            OSCAD_JSON_BINARY(BitwiseShiftRightOp, BitwiseShiftRightOp)
            OSCAD_JSON_BINARY(LogicalAndOp, LogicalAndOp)
            OSCAD_JSON_BINARY(LogicalOrOp, LogicalOrOp)
            OSCAD_JSON_BINARY(EqualityOp, EqualityOp)
            OSCAD_JSON_BINARY(InequalityOp, InequalityOp)
            OSCAD_JSON_BINARY(GreaterThanOp, GreaterThanOp)
            OSCAD_JSON_BINARY(GreaterThanOrEqualOp, GreaterThanOrEqualOp)
            OSCAD_JSON_BINARY(LessThanOp, LessThanOp)
            OSCAD_JSON_BINARY(LessThanOrEqualOp, LessThanOrEqualOp)
#undef OSCAD_JSON_BINARY

        case NodeKind::TernaryOp: {
            auto& n = static_cast<const TernaryOp&>(node);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["true_expr"] = valueToJson(n.trueExpr.get(), includePos);
            j["false_expr"] = valueToJson(n.falseExpr.get(), includePos);
            break;
        }

        case NodeKind::PrimaryCall: {
            auto& n = static_cast<const PrimaryCall&>(node);
            j["left"] = valueToJson(n.left.get(), includePos);
            j["arguments"] = listToJson(n.arguments, includePos);
            break;
        }
        case NodeKind::PrimaryIndex: {
            auto& n = static_cast<const PrimaryIndex&>(node);
            j["left"] = valueToJson(n.left.get(), includePos);
            j["index"] = valueToJson(n.index.get(), includePos);
            break;
        }
        case NodeKind::PrimaryMember: {
            auto& n = static_cast<const PrimaryMember&>(node);
            j["left"] = valueToJson(n.left.get(), includePos);
            j["member"] = valueToJson(n.member.get(), includePos);
            break;
        }

        case NodeKind::ListCompLet: {
            auto& n = static_cast<const ListCompLet&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::ListCompEach:
            j["body"] = valueToJson(static_cast<const ListCompEach&>(node).body.get(), includePos);
            break;
        case NodeKind::ListCompFor: {
            auto& n = static_cast<const ListCompFor&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::ListCompCFor: {
            auto& n = static_cast<const ListCompCFor&>(node);
            j["inits"] = listToJson(n.inits, includePos);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["incrs"] = listToJson(n.incrs, includePos);
            j["body"] = valueToJson(n.body.get(), includePos);
            break;
        }
        case NodeKind::ListCompIf: {
            auto& n = static_cast<const ListCompIf&>(node);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["true_expr"] = valueToJson(n.trueExpr.get(), includePos);
            break;
        }
        case NodeKind::ListCompIfElse: {
            auto& n = static_cast<const ListCompIfElse&>(node);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["true_expr"] = valueToJson(n.trueExpr.get(), includePos);
            j["false_expr"] = valueToJson(n.falseExpr.get(), includePos);
            break;
        }
        case NodeKind::ListComprehension:
            j["elements"] = listToJson(static_cast<const ListComprehension&>(node).elements, includePos);
            break;

        case NodeKind::RenderExpression: {
            auto& n = static_cast<const RenderExpression&>(node);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["children"] = listToJson(n.children, includePos);
            break;
        }

        case NodeKind::ModularCall: {
            auto& n = static_cast<const ModularCall&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["children"] = listToJson(n.children, includePos);
            break;
        }
        case NodeKind::ModularFor: {
            auto& n = static_cast<const ModularFor&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["body"] = listToJson(n.body, includePos);
            break;
        }
        case NodeKind::ModularIntersectionFor: {
            auto& n = static_cast<const ModularIntersectionFor&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["body"] = listToJson(n.body, includePos);
            break;
        }
        case NodeKind::ModularLet: {
            auto& n = static_cast<const ModularLet&>(node);
            j["assignments"] = listToJson(n.assignments, includePos);
            j["children"] = listToJson(n.children, includePos);
            break;
        }
        case NodeKind::ModularEcho: {
            auto& n = static_cast<const ModularEcho&>(node);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["children"] = listToJson(n.children, includePos);
            break;
        }
        case NodeKind::ModularAssert: {
            auto& n = static_cast<const ModularAssert&>(node);
            j["arguments"] = listToJson(n.arguments, includePos);
            j["children"] = listToJson(n.children, includePos);
            break;
        }
        case NodeKind::ModularIf: {
            auto& n = static_cast<const ModularIf&>(node);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["true_branch"] = listToJson(n.trueBranch, includePos);
            break;
        }
        case NodeKind::ModularIfElse: {
            auto& n = static_cast<const ModularIfElse&>(node);
            j["condition"] = valueToJson(n.condition.get(), includePos);
            j["true_branch"] = listToJson(n.trueBranch, includePos);
            j["false_branch"] = listToJson(n.falseBranch, includePos);
            break;
        }

#define OSCAD_JSON_MODIFIER(Kind, Type)                                                                                \
    case NodeKind::Kind:                                                                                               \
        j["child"] = valueToJson(static_cast<const Type&>(node).child.get(), includePos);                             \
        break;

            OSCAD_JSON_MODIFIER(ModularModifierShowOnly, ModularModifierShowOnly)
            OSCAD_JSON_MODIFIER(ModularModifierHighlight, ModularModifierHighlight)
            OSCAD_JSON_MODIFIER(ModularModifierBackground, ModularModifierBackground)
            OSCAD_JSON_MODIFIER(ModularModifierDisable, ModularModifierDisable)
#undef OSCAD_JSON_MODIFIER

        case NodeKind::ModuleDeclaration: {
            auto& n = static_cast<const ModuleDeclaration&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["parameters"] = listToJson(n.parameters, includePos);
            j["children"] = listToJson(n.children, includePos);
            j["pre_name_comments"] = listToJson(n.preNameComments, includePos);
            j["post_name_comments"] = listToJson(n.postNameComments, includePos);
            j["post_params_comments"] = listToJson(n.postParamsComments, includePos);
            break;
        }
        case NodeKind::FunctionDeclaration: {
            auto& n = static_cast<const FunctionDeclaration&>(node);
            j["name"] = valueToJson(n.name.get(), includePos);
            j["parameters"] = listToJson(n.parameters, includePos);
            j["expr"] = valueToJson(n.expr.get(), includePos);
            j["pre_name_comments"] = listToJson(n.preNameComments, includePos);
            j["post_name_comments"] = listToJson(n.postNameComments, includePos);
            j["post_params_comments"] = listToJson(n.postParamsComments, includePos);
            break;
        }
        case NodeKind::UseStatement:
            j["filepath"] = valueToJson(static_cast<const UseStatement&>(node).filepath.get(), includePos);
            break;
        case NodeKind::IncludeStatement:
            j["filepath"] = valueToJson(static_cast<const IncludeStatement&>(node).filepath.get(), includePos);
            break;
    }
    return j;
}

// -- fromJson: a Builder per NodeKind, keyed by nodeKindName() -------------

std::unique_ptr<ASTNode> buildIdentifier(const json& j, Position pos) {
    return std::make_unique<Identifier>(std::move(pos), j.at("name").get<std::string>());
}

const std::unordered_map<std::string, Builder>& registry() {
    static const std::unordered_map<std::string, Builder> table = {
        {"CommentLine",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<CommentLine>(std::move(pos), j.at("text").get<std::string>());
         }},
        {"BlankLine",
         [](const json&, Position pos) -> std::unique_ptr<ASTNode> { return std::make_unique<BlankLine>(std::move(pos)); }},
        {"CommentSpan",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<CommentSpan>(std::move(pos), j.at("text").get<std::string>());
         }},
        {"CommentedExpr",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<CommentedExpr>(std::move(pos), listFromJson<ASTNode>(j, "leading_comments"),
                                                     listFromJson<ASTNode>(j, "trailing_comments"),
                                                     childFromJson<Expression>(j, "expr"));
         }},
        {"Identifier", buildIdentifier},
        {"StringLiteral",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<StringLiteral>(std::move(pos), j.at("val").get<std::string>());
         }},
        {"NumberLiteral",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<NumberLiteral>(std::move(pos), j.at("val").get<double>());
         }},
        {"BooleanLiteral",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<BooleanLiteral>(std::move(pos), j.at("val").get<bool>());
         }},
        {"UndefinedLiteral",
         [](const json&, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<UndefinedLiteral>(std::move(pos));
         }},
        {"RangeLiteral",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<RangeLiteral>(std::move(pos), childFromJson<Expression>(j, "start"),
                                                    childFromJson<Expression>(j, "end"), childFromJson<Expression>(j, "step"),
                                                    j.value("implicitStep", false));
         }},
        {"ParameterDeclaration",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             auto node = std::make_unique<ParameterDeclaration>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                                 childFromJson<Expression>(j, "default"));
             node->leadingComments = listFromJson<CommentSpan>(j, "leading_comments");
             node->trailingComments = listFromJson<CommentSpan>(j, "trailing_comments");
             return node;
         }},
        {"PositionalArgument",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<PositionalArgument>(std::move(pos), childFromJson<Expression>(j, "expr"));
         }},
        {"NamedArgument",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<NamedArgument>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                     childFromJson<Expression>(j, "expr"));
         }},
        {"Assignment",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<Assignment>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                  childFromJson<Expression>(j, "expr"));
         }},
        {"LetOp",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<LetOp>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                             childFromJson<Expression>(j, "body"));
         }},
        {"EchoOp",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<EchoOp>(std::move(pos), listFromJson<Argument>(j, "arguments"),
                                              childFromJson<Expression>(j, "body"));
         }},
        {"AssertOp",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<AssertOp>(std::move(pos), listFromJson<Argument>(j, "arguments"),
                                                childFromJson<Expression>(j, "body"));
         }},
        {"FunctionLiteral",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<FunctionLiteral>(std::move(pos), listFromJson<ParameterDeclaration>(j, "parameters"),
                                                       childFromJson<Expression>(j, "body"));
         }},

#define OSCAD_JSON_BUILD_UNARY(Name)                                                                                   \
    {#Name, [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {                                             \
         return std::make_unique<Name>(std::move(pos), childFromJson<Expression>(j, "expr"));                          \
     }},

        OSCAD_JSON_BUILD_UNARY(UnaryMinusOp)
        OSCAD_JSON_BUILD_UNARY(LogicalNotOp)
        OSCAD_JSON_BUILD_UNARY(BitwiseNotOp)
#undef OSCAD_JSON_BUILD_UNARY

#define OSCAD_JSON_BUILD_BINARY(Name)                                                                                  \
    {#Name, [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {                                             \
         return std::make_unique<Name>(std::move(pos), childFromJson<Expression>(j, "left"),                           \
                                        childFromJson<Expression>(j, "right"));                                         \
     }},

        OSCAD_JSON_BUILD_BINARY(AdditionOp)
        OSCAD_JSON_BUILD_BINARY(SubtractionOp)
        OSCAD_JSON_BUILD_BINARY(MultiplicationOp)
        OSCAD_JSON_BUILD_BINARY(DivisionOp)
        OSCAD_JSON_BUILD_BINARY(ModuloOp)
        OSCAD_JSON_BUILD_BINARY(ExponentOp)
        OSCAD_JSON_BUILD_BINARY(BitwiseAndOp)
        OSCAD_JSON_BUILD_BINARY(BitwiseOrOp)
        OSCAD_JSON_BUILD_BINARY(BitwiseShiftLeftOp)
        OSCAD_JSON_BUILD_BINARY(BitwiseShiftRightOp)
        OSCAD_JSON_BUILD_BINARY(LogicalAndOp)
        OSCAD_JSON_BUILD_BINARY(LogicalOrOp)
        OSCAD_JSON_BUILD_BINARY(EqualityOp)
        OSCAD_JSON_BUILD_BINARY(InequalityOp)
        OSCAD_JSON_BUILD_BINARY(GreaterThanOp)
        OSCAD_JSON_BUILD_BINARY(GreaterThanOrEqualOp)
        OSCAD_JSON_BUILD_BINARY(LessThanOp)
        OSCAD_JSON_BUILD_BINARY(LessThanOrEqualOp)
#undef OSCAD_JSON_BUILD_BINARY

        {"TernaryOp",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<TernaryOp>(std::move(pos), childFromJson<Expression>(j, "condition"),
                                                 childFromJson<Expression>(j, "true_expr"),
                                                 childFromJson<Expression>(j, "false_expr"));
         }},

        {"PrimaryCall",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<PrimaryCall>(std::move(pos), childFromJson<Expression>(j, "left"),
                                                   listFromJson<Argument>(j, "arguments"));
         }},
        {"PrimaryIndex",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<PrimaryIndex>(std::move(pos), childFromJson<Expression>(j, "left"),
                                                    childFromJson<Expression>(j, "index"));
         }},
        {"PrimaryMember",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<PrimaryMember>(std::move(pos), childFromJson<Expression>(j, "left"),
                                                     childFromJson<Identifier>(j, "member"));
         }},

        {"ListCompLet",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompLet>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                                   childFromJson<ASTNode>(j, "body"));
         }},
        {"ListCompEach",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompEach>(std::move(pos), childFromJson<ASTNode>(j, "body"));
         }},
        {"ListCompFor",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompFor>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                                   childFromJson<ASTNode>(j, "body"));
         }},
        {"ListCompCFor",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompCFor>(std::move(pos), listFromJson<Assignment>(j, "inits"),
                                                    childFromJson<Expression>(j, "condition"),
                                                    listFromJson<Assignment>(j, "incrs"), childFromJson<ASTNode>(j, "body"));
         }},
        {"ListCompIf",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompIf>(std::move(pos), childFromJson<Expression>(j, "condition"),
                                                  childFromJson<ASTNode>(j, "true_expr"));
         }},
        {"ListCompIfElse",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListCompIfElse>(std::move(pos), childFromJson<Expression>(j, "condition"),
                                                      childFromJson<ASTNode>(j, "true_expr"),
                                                      childFromJson<ASTNode>(j, "false_expr"));
         }},
        {"ListComprehension",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ListComprehension>(std::move(pos), listFromJson<ASTNode>(j, "elements"));
         }},

        {"RenderExpression",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<RenderExpression>(std::move(pos), listFromJson<Argument>(j, "arguments"),
                                                        listFromJson<ASTNode>(j, "children"));
         }},
        {"ModularCall",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularCall>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                   listFromJson<Argument>(j, "arguments"), listFromJson<ASTNode>(j, "children"));
         }},
        {"ModularFor",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularFor>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                                  listFromJson<ASTNode>(j, "body"));
         }},
        {"ModularIntersectionFor",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularIntersectionFor>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                                              listFromJson<ASTNode>(j, "body"));
         }},
        {"ModularLet",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularLet>(std::move(pos), listFromJson<Assignment>(j, "assignments"),
                                                  listFromJson<ASTNode>(j, "children"));
         }},
        {"ModularEcho",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularEcho>(std::move(pos), listFromJson<Argument>(j, "arguments"),
                                                   listFromJson<ASTNode>(j, "children"));
         }},
        {"ModularAssert",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularAssert>(std::move(pos), listFromJson<Argument>(j, "arguments"),
                                                     listFromJson<ASTNode>(j, "children"));
         }},
        {"ModularIf",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularIf>(std::move(pos), childFromJson<Expression>(j, "condition"),
                                                 listFromJson<ASTNode>(j, "true_branch"));
         }},
        {"ModularIfElse",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<ModularIfElse>(std::move(pos), childFromJson<Expression>(j, "condition"),
                                                     listFromJson<ASTNode>(j, "true_branch"),
                                                     listFromJson<ASTNode>(j, "false_branch"));
         }},

#define OSCAD_JSON_BUILD_MODIFIER(Name)                                                                                \
    {#Name, [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {                                             \
         return std::make_unique<Name>(std::move(pos), childFromJson<ModuleInstantiation>(j, "child"));                \
     }},

        OSCAD_JSON_BUILD_MODIFIER(ModularModifierShowOnly)
        OSCAD_JSON_BUILD_MODIFIER(ModularModifierHighlight)
        OSCAD_JSON_BUILD_MODIFIER(ModularModifierBackground)
        OSCAD_JSON_BUILD_MODIFIER(ModularModifierDisable)
#undef OSCAD_JSON_BUILD_MODIFIER

        {"ModuleDeclaration",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             auto node = std::make_unique<ModuleDeclaration>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                              listFromJson<ParameterDeclaration>(j, "parameters"),
                                                              listFromJson<ASTNode>(j, "children"));
             node->preNameComments = listFromJson<CommentSpan>(j, "pre_name_comments");
             node->postNameComments = listFromJson<CommentSpan>(j, "post_name_comments");
             node->postParamsComments = listFromJson<CommentSpan>(j, "post_params_comments");
             return node;
         }},
        {"FunctionDeclaration",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             auto node = std::make_unique<FunctionDeclaration>(std::move(pos), childFromJson<Identifier>(j, "name"),
                                                                listFromJson<ParameterDeclaration>(j, "parameters"),
                                                                childFromJson<Expression>(j, "expr"));
             node->preNameComments = listFromJson<CommentSpan>(j, "pre_name_comments");
             node->postNameComments = listFromJson<CommentSpan>(j, "post_name_comments");
             node->postParamsComments = listFromJson<CommentSpan>(j, "post_params_comments");
             return node;
         }},
        {"UseStatement",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<UseStatement>(std::move(pos), childFromJson<StringLiteral>(j, "filepath"));
         }},
        {"IncludeStatement",
         [](const json& j, Position pos) -> std::unique_ptr<ASTNode> {
             return std::make_unique<IncludeStatement>(std::move(pos), childFromJson<StringLiteral>(j, "filepath"));
         }},
    };
    return table;
}

} // namespace

json astToJson(const ASTNode& node, bool includePosition) {
    return toJsonImpl(node, includePosition);
}

json astToJson(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition) {
    json arr = json::array();
    for (const auto& n : nodes) {
        arr.push_back(astToJson(*n, includePosition));
    }
    return arr;
}

std::string astToJsonString(const std::vector<std::unique_ptr<ASTNode>>& nodes, bool includePosition, int indent) {
    return astToJson(nodes, includePosition).dump(indent);
}

std::unique_ptr<ASTNode> astFromJson(const json& data) {
    if (!data.contains("_type")) {
        throw std::runtime_error("Missing '_type' field in node data");
    }
    std::string typeName = data.at("_type").get<std::string>();
    const auto& table = registry();
    auto it = table.find(typeName);
    if (it == table.end()) {
        throw std::runtime_error("Unknown node type: " + typeName);
    }
    return it->second(data, positionFromJson(data));
}

std::vector<std::unique_ptr<ASTNode>> astFromJsonArray(const json& data) {
    std::vector<std::unique_ptr<ASTNode>> result;
    for (const auto& item : data) {
        result.push_back(astFromJson(item));
    }
    return result;
}

std::vector<std::unique_ptr<ASTNode>> astFromJsonString(const std::string& jsonStr) {
    return astFromJsonArray(json::parse(jsonStr));
}

} // namespace oscad
