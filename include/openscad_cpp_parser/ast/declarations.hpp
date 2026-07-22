#pragma once

#include "openscad_cpp_parser/ast/expression.hpp"

#include <memory>
#include <vector>

namespace oscad {

// `children` is heterogeneous (ModuleInstantiation | Assignment |
// FunctionDeclaration | ModuleDeclaration), matching the Python union type
// exactly -- no need for a variant, just the common ASTNode base, with the
// concrete type recovered via kind()/dynamic_cast at point of use.
class ModuleDeclaration : public ASTNode {
public:
    ModuleDeclaration(Position position, std::unique_ptr<Identifier> name,
                       std::vector<std::unique_ptr<ParameterDeclaration>> parameters,
                       std::vector<std::unique_ptr<ASTNode>> children)
        : ASTNode(NodeKind::ModuleDeclaration, std::move(position)), name(std::move(name)),
          parameters(std::move(parameters)), children(std::move(children)) {}

    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<ParameterDeclaration>> parameters;
    std::vector<std::unique_ptr<ASTNode>> children;
    std::vector<std::unique_ptr<CommentSpan>> preNameComments;
    std::vector<std::unique_ptr<CommentSpan>> postNameComments;
    std::vector<std::unique_ptr<CommentSpan>> postParamsComments;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class FunctionDeclaration : public ASTNode {
public:
    FunctionDeclaration(Position position, std::unique_ptr<Identifier> name,
                         std::vector<std::unique_ptr<ParameterDeclaration>> parameters, std::unique_ptr<Expression> expr)
        : ASTNode(NodeKind::FunctionDeclaration, std::move(position)), name(std::move(name)),
          parameters(std::move(parameters)), expr(std::move(expr)) {}

    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<ParameterDeclaration>> parameters;
    std::unique_ptr<Expression> expr;
    std::vector<std::unique_ptr<CommentSpan>> preNameComments;
    std::vector<std::unique_ptr<CommentSpan>> postNameComments;
    std::vector<std::unique_ptr<CommentSpan>> postParamsComments;

    std::string toString() const override;
    void buildScope(Scope& parentScope) override;
};

class UseStatement : public ASTNode {
public:
    UseStatement(Position position, std::unique_ptr<StringLiteral> filepath)
        : ASTNode(NodeKind::UseStatement, std::move(position)), filepath(std::move(filepath)) {}

    std::unique_ptr<StringLiteral> filepath; // val holds the path without surrounding < >

    std::string toString() const override { return "use <" + filepath->val + ">"; }
};

class IncludeStatement : public ASTNode {
public:
    IncludeStatement(Position position, std::unique_ptr<StringLiteral> filepath)
        : ASTNode(NodeKind::IncludeStatement, std::move(position)), filepath(std::move(filepath)) {}

    std::unique_ptr<StringLiteral> filepath; // val holds the path without surrounding < >

    std::string toString() const override { return "include <" + filepath->val + ">"; }
};

} // namespace oscad
