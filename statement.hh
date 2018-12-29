/*
 *	Copyright © 2016 Flamewing <flamewing.sonic@gmail.com>
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STATEMENT_HH
#define STATEMENT_HH

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "expression.hh"

class driver;

// Generic statement
class Statement {
public:
    Statement() noexcept            = default;
    virtual ~Statement() noexcept   = default;
    Statement(Statement const&)     = default;
    Statement(Statement&&) noexcept = default;
    Statement& operator=(Statement const&) = default;
    Statement& operator=(Statement&&) noexcept = default;

    std::ostream& write(std::ostream& out, size_t indent) const noexcept {
        return write_impl(out, indent);
    }
    std::unique_ptr<Statement> clone() const {
        return std::unique_ptr<Statement>(clone_impl());
    }
    std::string getIndent(size_t indent) const {
        return std::string(indent, ' ');
    }

private:
    virtual gsl::owner<Statement*> clone_impl() const = 0;
    virtual bool                   is_simple() const noexcept { return true; }

protected:
    virtual std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept = 0;
};

// A variable assignment statement
class SetStatement : public Statement {
public:
    explicit SetStatement(std::string name) : varName(std::move(name)) {}

private:
    gsl::owner<SetStatement*> clone_impl() const override {
        return new SetStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        return out << getIndent(indent) << "~ " << varName;
    }

private:
    std::string varName;
};

// A generic statement containing an expression
class ExpressionStatement : public Statement {
public:
    explicit ExpressionStatement(std::unique_ptr<Expression> expr)
        : expression(std::move(expr)) {}
    ~ExpressionStatement() noexcept override = default;
    ExpressionStatement(ExpressionStatement const& other)
        : Statement(*this), expression(other.expression->clone()) {}
    ExpressionStatement(ExpressionStatement&& other) noexcept = default;
    ExpressionStatement& operator=(ExpressionStatement const& other) {
        if (this != &other) {
            Statement::operator=(other);
            expression         = other.expression->clone();
        }
        return *this;
    }
    ExpressionStatement&
    operator=(ExpressionStatement&& other) noexcept = default;

private:
    gsl::owner<ExpressionStatement*> clone_impl() const override {
        return new ExpressionStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        out << getIndent(indent) << "~ ";
        return expression->write(out) << '\n';
    }

private:
    std::unique_ptr<Expression> expression;
};

// A generic statement block statement
class BlockStatement : public Statement {
public:
    using StatementList = std::vector<std::unique_ptr<Statement>>;
    BlockStatement()    = default;
    ~BlockStatement() noexcept override = default;
    BlockStatement(BlockStatement const& other) : Statement(other) {
        statements.reserve(other.statements.size());
        for (auto const& elem : other.statements) {
            statements.emplace_back(elem->clone());
        }
    }
    // noexcept(true) wo work around clang-tidy bug
    BlockStatement(BlockStatement&&) noexcept(true) = default;
    BlockStatement& operator=(BlockStatement const& other) {
        if (this != &other) {
            Statement::operator=(other);
            statements.clear();
            statements.reserve(other.statements.size());
            for (auto const& elem : other.statements) {
                statements.emplace_back(elem->clone());
            }
        }
        return *this;
    }
    BlockStatement& operator=(BlockStatement&&) noexcept = default;

    void add_statement(std::unique_ptr<Statement> stmt) {
        statements.emplace_back(std::move(stmt));
    }
    void steal_statements(StatementList& other) noexcept {
        statements.swap(other);
    }
    StatementList& get_statements() noexcept { return statements; }

private:
    gsl::owner<BlockStatement*> clone_impl() const override {
        return new BlockStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        for (auto const& elem : statements) {
            elem->write(out, indent);
        }
        return out;
    }

private:
    StatementList statements;
};

// An if statement
class ElseStatement : public BlockStatement {
public:
    ElseStatement() = default;

private:
    gsl::owner<ElseStatement*> clone_impl() const override {
        return new ElseStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        out << getIndent(indent) << "- else:\n";
        return BlockStatement::write_impl(out, indent + 4);
    }
};

// An if statement
class IfStatement : public Statement {
public:
    IfStatement(
        std::unique_ptr<Expression> cond, std::unique_ptr<Statement> then)
        : condExpr(std::move(cond)), thenStmt(std::move(then)), elseStmt() {}
    IfStatement(
        std::unique_ptr<Expression> cond, std::unique_ptr<Statement> then,
        std::unique_ptr<Statement> else_)
        : condExpr(std::move(cond)), thenStmt(std::move(then)),
          elseStmt(std::move(else_)) {}
    ~IfStatement() noexcept override = default;
    IfStatement(IfStatement const& other)
        : Statement(*this), condExpr(other.condExpr->clone()),
          thenStmt(other.thenStmt->clone()),
          elseStmt(other.elseStmt ? other.elseStmt->clone() : nullptr) {}
    IfStatement(IfStatement&& other) noexcept = default;

    IfStatement& operator=(IfStatement const& other) {
        if (this != &other) {
            Statement::operator=(other);
            condExpr           = other.condExpr->clone();
            thenStmt           = other.thenStmt->clone();
            if (other.elseStmt) {
                elseStmt = other.elseStmt->clone();
            } else {
                elseStmt.reset();
            }
        }
        return *this;
    }
    IfStatement& operator=(IfStatement&& other) noexcept = default;

private:
    gsl::owner<IfStatement*> clone_impl() const override {
        return new IfStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        out << getIndent(indent) << "- ";
        condExpr->write(out) << "\n";
        thenStmt->write(out, indent + 4);
        if (elseStmt) {
            elseStmt->write(out, indent) << "\n";
        }
        return out;
    }

private:
    std::unique_ptr<Expression> condExpr;
    std::unique_ptr<Statement>  thenStmt;
    std::unique_ptr<Statement>  elseStmt;
};

// A global variable definition
class GlobalVariableStatement final : public Statement {
public:
    GlobalVariableStatement() noexcept = default;
    explicit GlobalVariableStatement(std::string name, std::string value)
        : varName(std::move(name)), varValue(std::move(value)) {
        add_global(varName);
    }

    static bool add_global(std::string var) {
        return variables.insert(std::move(var)).second;
    }
    static bool is_global(const std::string& var) {
        return variables.find(var) != variables.cend();
    }

private:
    // Global variables
    static inline std::set<std::string> variables;

    gsl::owner<GlobalVariableStatement*> clone_impl() const override {
        return new GlobalVariableStatement(*this);
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t) const noexcept final {
        return out << "VAR " << varName << " = " << varValue << '\n';
    }

private:
    std::string varName;
    std::string varValue;
};

// Base for knots, stitches, and functions
class TopLevelStatement : public BlockStatement {
public:
    TopLevelStatement(std::string name_, driver& drv_)
        : drv(&drv_), name(std::move(name_)) {}
    std::string const& getName() const noexcept { return name; }

private:
    gsl::owner<TopLevelStatement*> clone_impl() const override {
        return new TopLevelStatement(*this);
    }

protected:
    std::ostream& write_header_base(std::ostream& out) const noexcept {
        out << name;
        if (!headerVariables.empty()) {
            out << '(';
            bool firstVar = true;
            for (auto const& [varName, isRef] : headerVariables) {
                if (!firstVar) {
                    out << ", ";
                    firstVar = true;
                }
                if (isRef) {
                    out << "ref ";
                }
                out << varName;
            }
            out << ')';
        }
        return out;
    }
    virtual std::ostream& write_header(std::ostream& out) const noexcept {
        return out;
    }

    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept override {
        write_header(out);
        return BlockStatement::write_impl(out, indent) << '\n';
    }

private:
    driver*                     drv;
    std::string                 name;
    std::map<std::string, bool> headerVariables;
};

// Class representing a stitch and all its statements
class StitchStatement final : public TopLevelStatement {
public:
    StitchStatement(std::string const& name_, driver& drv_)
        : TopLevelStatement(name_, drv_) {}

private:
    gsl::owner<StitchStatement*> clone_impl() const override {
        return new StitchStatement(*this);
    }
    std::ostream& write_header(std::ostream& out) const noexcept final {
        out << "= ";
        return write_header_base(out) << '\n';
    }
};

// Class representing a knot and all its stitches and statements
class KnotStatement final : public TopLevelStatement {
public:
    KnotStatement(std::string const& name_, driver& drv_)
        : TopLevelStatement(name_, drv_) {}
    ~KnotStatement() noexcept override = default;
    KnotStatement(KnotStatement const& other) : TopLevelStatement(other) {
        stitches.reserve(other.stitches.size());
        for (auto const& elem : other.stitches) {
            stitches.emplace_back(
                static_unique_pointer_cast<StitchStatement>(elem->clone()));
        }
    }
    KnotStatement(KnotStatement&&) noexcept = default;

    KnotStatement& operator=(KnotStatement const& other) {
        if (this != &other) {
            TopLevelStatement::operator=(other);
            stitches.clear();
            stitches.reserve(other.stitches.size());
            for (auto const& elem : other.stitches) {
                stitches.emplace_back(
                    static_unique_pointer_cast<StitchStatement>(elem->clone()));
            }
        }
        return *this;
    }
    KnotStatement& operator=(KnotStatement&&) noexcept = default;

    void add_stitch(std::unique_ptr<StitchStatement> stitch) {
        if (stitch->getName() == getName()) {
            steal_statements(stitch->get_statements());
        } else {
            stitches.emplace_back(std::move(stitch));
        }
    }

private:
    gsl::owner<KnotStatement*> clone_impl() const override {
        return new KnotStatement(*this);
    }
    std::ostream& write_header(std::ostream& out) const noexcept final {
        out << "=== ";
        return write_header_base(out) << " ===\n";
    }

protected:
    std::ostream& write_impl(std::ostream& out, size_t indent) const
        noexcept final {
        write_header(out);
        TopLevelStatement::write_impl(out, indent) << '\n';
        for (auto const& elem : stitches) {
            elem->write(out, indent);
        }
        return out << '\n';
    }

private:
    std::vector<std::unique_ptr<StitchStatement>> stitches;
};

// Class representing a function and all its statements
class FunctionStatement final : public TopLevelStatement {
public:
    FunctionStatement(std::string const& name_, driver& drv_)
        : TopLevelStatement(name_, drv_) {}

private:
    gsl::owner<FunctionStatement*> clone_impl() const override {
        return new FunctionStatement(*this);
    }
    std::ostream& write_header(std::ostream& out) const noexcept final {
        out << "=== function ";
        return write_header_base(out) << " ===\n";
    }
};

#endif
