#include "openscad_cpp_parser/ast/ast_node.hpp"

namespace oscad {

const char* nodeKindName(NodeKind kind) {
    switch (kind) {
        case NodeKind::CommentLine: return "CommentLine";
        case NodeKind::BlankLine: return "BlankLine";
        case NodeKind::CommentSpan: return "CommentSpan";
        case NodeKind::CommentedExpr: return "CommentedExpr";
        case NodeKind::Identifier: return "Identifier";
        case NodeKind::StringLiteral: return "StringLiteral";
        case NodeKind::NumberLiteral: return "NumberLiteral";
        case NodeKind::BooleanLiteral: return "BooleanLiteral";
        case NodeKind::UndefinedLiteral: return "UndefinedLiteral";
        case NodeKind::RangeLiteral: return "RangeLiteral";
        case NodeKind::ParameterDeclaration: return "ParameterDeclaration";
        case NodeKind::PositionalArgument: return "PositionalArgument";
        case NodeKind::NamedArgument: return "NamedArgument";
        case NodeKind::Assignment: return "Assignment";
        case NodeKind::LetOp: return "LetOp";
        case NodeKind::EchoOp: return "EchoOp";
        case NodeKind::AssertOp: return "AssertOp";
        case NodeKind::FunctionLiteral: return "FunctionLiteral";
        case NodeKind::UnaryMinusOp: return "UnaryMinusOp";
        case NodeKind::AdditionOp: return "AdditionOp";
        case NodeKind::SubtractionOp: return "SubtractionOp";
        case NodeKind::MultiplicationOp: return "MultiplicationOp";
        case NodeKind::DivisionOp: return "DivisionOp";
        case NodeKind::ModuloOp: return "ModuloOp";
        case NodeKind::ExponentOp: return "ExponentOp";
        case NodeKind::BitwiseAndOp: return "BitwiseAndOp";
        case NodeKind::BitwiseOrOp: return "BitwiseOrOp";
        case NodeKind::BitwiseNotOp: return "BitwiseNotOp";
        case NodeKind::BitwiseShiftLeftOp: return "BitwiseShiftLeftOp";
        case NodeKind::BitwiseShiftRightOp: return "BitwiseShiftRightOp";
        case NodeKind::LogicalAndOp: return "LogicalAndOp";
        case NodeKind::LogicalOrOp: return "LogicalOrOp";
        case NodeKind::LogicalNotOp: return "LogicalNotOp";
        case NodeKind::TernaryOp: return "TernaryOp";
        case NodeKind::EqualityOp: return "EqualityOp";
        case NodeKind::InequalityOp: return "InequalityOp";
        case NodeKind::GreaterThanOp: return "GreaterThanOp";
        case NodeKind::GreaterThanOrEqualOp: return "GreaterThanOrEqualOp";
        case NodeKind::LessThanOp: return "LessThanOp";
        case NodeKind::LessThanOrEqualOp: return "LessThanOrEqualOp";
        case NodeKind::PrimaryCall: return "PrimaryCall";
        case NodeKind::PrimaryIndex: return "PrimaryIndex";
        case NodeKind::PrimaryMember: return "PrimaryMember";
        case NodeKind::ListCompLet: return "ListCompLet";
        case NodeKind::ListCompEach: return "ListCompEach";
        case NodeKind::ListCompFor: return "ListCompFor";
        case NodeKind::ListCompCFor: return "ListCompCFor";
        case NodeKind::ListCompIf: return "ListCompIf";
        case NodeKind::ListCompIfElse: return "ListCompIfElse";
        case NodeKind::ListComprehension: return "ListComprehension";
        case NodeKind::ModularCall: return "ModularCall";
        case NodeKind::ModularFor: return "ModularFor";
        case NodeKind::ModularIntersectionFor: return "ModularIntersectionFor";
        case NodeKind::ModularLet: return "ModularLet";
        case NodeKind::ModularEcho: return "ModularEcho";
        case NodeKind::ModularAssert: return "ModularAssert";
        case NodeKind::ModularIf: return "ModularIf";
        case NodeKind::ModularIfElse: return "ModularIfElse";
        case NodeKind::ModularModifierShowOnly: return "ModularModifierShowOnly";
        case NodeKind::ModularModifierHighlight: return "ModularModifierHighlight";
        case NodeKind::ModularModifierBackground: return "ModularModifierBackground";
        case NodeKind::ModularModifierDisable: return "ModularModifierDisable";
        case NodeKind::ModuleDeclaration: return "ModuleDeclaration";
        case NodeKind::FunctionDeclaration: return "FunctionDeclaration";
        case NodeKind::UseStatement: return "UseStatement";
        case NodeKind::IncludeStatement: return "IncludeStatement";
    }
    return "Unknown";
}

} // namespace oscad
