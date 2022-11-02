
#include <string>
#include <iosfwd>
#include <iostream>
#include <set>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include "Absyn.H"
#include "Skeleton.C"

class JVM_Var_Init_Visitor : public Skeleton {
public:
    int elem_count = 1;
    std::map<Ident, int> var_table = std::map<Ident, int>();

    void visitSAss(SAss *s_ass) override {
        if (var_table.find(s_ass->ident_) == var_table.end()) {
            var_table[s_ass->ident_] = elem_count;
            elem_count++;
        }
    };

};

class JVM_Stack_Limit_Visitor : public Skeleton {
public:
    int max_stack_usage = 0;
    int current_stack_usage = 0;

    void visitSAss(SAss *s_ass) override {
        current_stack_usage = 0;
        if (s_ass->exp_) s_ass->exp_->accept(this);
    }

    void visitSExp(SExp *s_exp) override {
        current_stack_usage = 0;
        if (s_exp->exp_) s_exp->exp_->accept(this);
    }

    void visitExpLit(ExpLit *exp_lit) override {
        current_stack_usage++;
        max_stack_usage = std::max(max_stack_usage, current_stack_usage);
    }

    void visitExpVar(ExpVar *exp_var) override {
        current_stack_usage++;
        max_stack_usage = std::max(max_stack_usage, current_stack_usage);
    }

    void visitBinaryExp(Exp *binary, Exp *exp_1, Exp *exp_2) {
        int max_stack_usage_before = max_stack_usage;
        current_stack_usage = 0;
        max_stack_usage = 0;

        if (exp_1) exp_1->accept(this);
        exp_1->height = max_stack_usage;
        current_stack_usage = 0;
        max_stack_usage = 0;

        if (exp_2) exp_2->accept(this);
        exp_2->height = max_stack_usage;

        if (std::max(exp_1->height, exp_2->height) >
            std::min(exp_1->height, exp_2->height)) {
            // one of the paths is harder, can optimize by doing harder first
            current_stack_usage = std::max(exp_1->height, exp_2->height);
        } else {
            // paths equally hard, cannot optimize
            current_stack_usage = std::max(exp_1->height, exp_2->height) + 1;
        }
        binary->height = current_stack_usage;
        max_stack_usage = std::max(max_stack_usage_before, current_stack_usage);
    }

    void visitExpAdd(ExpAdd *exp_add) override {
        visitBinaryExp(exp_add, exp_add->exp_1, exp_add->exp_2);
    }

    void visitExpSub(ExpSub *exp_sub) override {
        visitBinaryExp(exp_sub, exp_sub->exp_1, exp_sub->exp_2);
    }

    void visitExpMul(ExpMul *exp_mul) override {
        visitBinaryExp(exp_mul, exp_mul->exp_1, exp_mul->exp_2);
    }

    void visitExpDiv(ExpDiv *exp_div) override {
        visitBinaryExp(exp_div, exp_div->exp_1, exp_div->exp_2);
    }
};

class JVM_Compile_Visitor : public Skeleton {
private:
    static std::string int_to_jvm_bytecode(int value) {
        if (value == -1) {
            return "\ticonst_m1\n";
        } else if (value >= 0 && value <= 5) {
            return "\ticonst_" + std::to_string(value) + "\n";
        } else if (value >= INT8_MIN && value <= INT8_MAX) {
            return "\tbipush " + std::to_string(value) + "\n";
        } else if (value >= INT16_MIN && value <= INT16_MAX) {
            return "\tsipush " + std::to_string(value) + "\n";
        } else {
            return "\tldc " + std::to_string(value) + "\n";
        }
    }

    void append_print_invoke() {
        output_file << "\tinvokestatic Runtime/printInt(I)V\n";
    }

    void append_swap() {
        output_file << "\tswap\n";
    }

public:
    std::ofstream &output_file;
    std::map<Ident, int> &var_table;

    explicit JVM_Compile_Visitor(std::ofstream &output_file, std::map<Ident, int> &var_table)
            : output_file(output_file),
              var_table(var_table) {}

    void visitExpLit(ExpLit *exp_lit) override {
        output_file << int_to_jvm_bytecode(exp_lit->integer_);
    }

    void visitExpVar(ExpVar *exp_var) override {
        if (var_table.find(exp_var->ident_) != var_table.end()) {
            int var_num = var_table[exp_var->ident_];
            output_file << std::string("\tiload") + (var_num < 4 ? "_" : " ") +
                           std::to_string(var_num) + "\n";
        } else {
            output_file << int_to_jvm_bytecode(0);
        }
    }

    void visitSAss(SAss *s_ass) override {
        if (s_ass->exp_) s_ass->exp_->accept(this);

        int var_num = var_table[s_ass->ident_];
        output_file << (std::string("\tistore") + (var_num < 4 ? "_" : " ") +
                        std::to_string(var_num) + "\n");

    };

    void visitSExp(SExp *s_exp) override {
        if (s_exp->exp_) s_exp->exp_->accept(this);

        append_print_invoke();
    }

    void visitBinaryExp(bool order_matters, Exp *exp_1, Exp *exp_2) {
        if (exp_1->height >= exp_2->height) {
            exp_1->accept(this);
            exp_2->accept(this);
        } else {
            exp_2->accept(this);
            exp_1->accept(this);
            if (order_matters) {
                append_swap();
            }
        }
    }

    void visitExpAdd(ExpAdd *exp_add) override {
        visitBinaryExp(false, exp_add->exp_1, exp_add->exp_2);
        output_file << "\tiadd\n";
    };

    void visitExpMul(ExpMul *exp_mul) override {
        visitBinaryExp(false, exp_mul->exp_1, exp_mul->exp_2);
        output_file << "\timul\n";
    };

    void visitExpSub(ExpSub *exp_sub) override {
        visitBinaryExp(true, exp_sub->exp_1, exp_sub->exp_2);
        output_file << "\tisub\n";
    };

    void visitExpDiv(ExpDiv *exp_div) override {
        visitBinaryExp(true, exp_div->exp_1, exp_div->exp_2);

        output_file << "\tidiv\n";
    };
};

class JVM_Compiler {
private:
    Program *program;
    std::string &filename_no_extension;
    std::ofstream output_file;
    std::map<Ident, int> var_table;

    void init_common() {
        output_file << ".source " + filename_no_extension + ".j\n" +
                       ".class public " + filename_no_extension + "\n" +
                       ".super java/lang/Object\n"
                       "\n"
                       ".method public <init>()V\n"
                       "\taload_0\n"
                       "\tinvokespecial java/lang/Object/<init>()V\n"
                       "\treturn\n"
                       ".end method\n"
                       "\n"
                       ".method public static main([Ljava/lang/String;)V\n";
    }

    void close_common() {
        output_file << "\treturn\n"
                       ".end method\n";
        output_file.close();
    }

public:
    explicit JVM_Compiler(Program *program, std::string &filename_no_extension, const std::filesystem::path &file_path)
            : program(program), filename_no_extension(filename_no_extension) {
        output_file.open(file_path);
        if (!output_file.is_open()) {
            std::cerr << "can't open output file" << std::endl;
            exit(1);
        }
    }

    void compile() {
        init_common();
        auto var_init_visitor = new JVM_Var_Init_Visitor;
        program->accept(var_init_visitor);
        var_table = var_init_visitor->var_table;
        output_file << "\t.limit locals " + std::to_string(var_table.size() + 1) + "\n"; // + 1 for argument (main)

        auto stack_limit_visitor = new JVM_Stack_Limit_Visitor;
        program->accept(stack_limit_visitor);
        output_file << "\t.limit stack " + std::to_string(stack_limit_visitor->max_stack_usage) + "\n";

        auto compile_visitor = new JVM_Compile_Visitor(output_file, var_table);
        program->accept(compile_visitor);

        delete (var_init_visitor);
        delete (stack_limit_visitor);
        delete (compile_visitor);
        close_common();
    }
};