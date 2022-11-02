
#include <string>
#include <iosfwd>
#include <iostream>
#include <set>
#include <fstream>
#include <vector>
#include "Absyn.H"
#include "Skeleton.C"

class LLVM_Var_Init_Visitor : public Skeleton {
public:
    std::set<Ident> variables = std::set<Ident>();

    void visitSAss(SAss *s_ass) override { variables.insert(s_ass->ident_); };

};

class LLVM_Compile_Visitor : public Skeleton {
public:
    std::ofstream &output_file;
    std::string top_exp_result;
    int tmp_register_number = 0;
    std::set<Ident> variables;

    explicit LLVM_Compile_Visitor(std::ofstream &output_file, std::set<Ident> &variables) : output_file(output_file),
                                                                                            variables(variables) {}

    void call_print(const std::string &value) {
        // value can be registry (%x) or const (42)
        output_file << "\tcall void @printInt(i32 " + value + ")\n";
    }

    void store(const std::string &value, const std::string &variable) {
        // value can be registry (%x) or const (42)
        output_file << "\tstore i32 " + value + ", i32* " + variable + "\n";
    }


    void visitSAss(SAss *s_ass) override {
        if (s_ass->exp_) s_ass->exp_->accept(this);

        store(top_exp_result, "%" + s_ass->ident_);
    };

    void visitSExp(SExp *s_exp) override {
        if (s_exp->exp_) s_exp->exp_->accept(this);

        call_print(top_exp_result);
    }

    void visitExpAdd(ExpAdd *exp_add) override {
        if (exp_add->exp_1) exp_add->exp_1->accept(this);
        std::string result_1 = top_exp_result;

        if (exp_add->exp_2) exp_add->exp_2->accept(this);
        std::string result_2 = top_exp_result;

        int register_num = tmp_register_number;
        tmp_register_number++;
        output_file
                << "\t%t_" + std::to_string(register_num) + " = add i32 " + result_1 + ", " + result_2 + "\n";

        top_exp_result = "%t_" + std::to_string(register_num);
    };

    void visitExpSub(ExpSub *exp_sub) override {
        if (exp_sub->exp_1) exp_sub->exp_1->accept(this);
        std::string result_1 = top_exp_result;

        if (exp_sub->exp_2) exp_sub->exp_2->accept(this);
        std::string result_2 = top_exp_result;

        int register_num = tmp_register_number;
        tmp_register_number++;
        output_file
                << "\t%t_" + std::to_string(register_num) + " = sub i32 " + result_1 + ", " + result_2 + "\n";

        top_exp_result = "%t_" + std::to_string(register_num);
    };

    void visitExpMul(ExpMul *exp_mul) override {
        if (exp_mul->exp_1) exp_mul->exp_1->accept(this);
        std::string result_1 = top_exp_result;

        if (exp_mul->exp_2) exp_mul->exp_2->accept(this);
        std::string result_2 = top_exp_result;

        int register_num = tmp_register_number;
        tmp_register_number++;
        output_file
                << "\t%t_" + std::to_string(register_num) + " = mul i32 " + result_1 + ", " + result_2 + "\n";

        top_exp_result = "%t_" + std::to_string(register_num);

    };

    void visitExpDiv(ExpDiv *exp_div) override {
        if (exp_div->exp_1) exp_div->exp_1->accept(this);
        std::string result_1 = top_exp_result;

        if (exp_div->exp_2) exp_div->exp_2->accept(this);
        std::string result_2 = top_exp_result;

        int register_num = tmp_register_number;
        tmp_register_number++;
        output_file
                << "\t%t_" + std::to_string(register_num) + " = sdiv i32 " + result_1 + ", " + result_2 + "\n";

        top_exp_result = "%t_" + std::to_string(register_num);
    };

    void visitExpLit(ExpLit *exp_lit) override {
        top_exp_result = std::to_string(exp_lit->integer_);
    }

    void visitExpVar(ExpVar *exp_var) override {
        if (variables.find(exp_var->ident_) != variables.end()) {
            int register_num = tmp_register_number;
            tmp_register_number++;
            output_file << "\t%t_" + std::to_string(register_num) + " = load i32, i32* %" + exp_var->ident_ + "\n";
            top_exp_result = "%t_" + std::to_string(register_num);
        } else {
            top_exp_result = "0";
        }
    }
};

class LLVM_Compiler {
private:
    Program *program;
    std::ofstream output_file;
    std::set<Ident> variables;

    void init_common() {
        output_file << "@dnl = internal constant [4 x i8] c\"%d\\0A\\00\"\n\n"
                       "declare i32 @printf(i8*, ...) \n"
                       "\n"
                       "define void @printInt(i32 %x) {\n"
                       "       %t0 = getelementptr [4 x i8], [4 x i8]* @dnl, i32 0, i32 0\n"
                       "       call i32 (i8*, ...) @printf(i8* %t0, i32 %x) \n"
                       "       ret void\n"
                       "}\n\n"
                       "define i32 @main(i32 %argc, i8** %argv) {\n";
    }

    void close_common() {
        output_file << "\tret i32 0\n"
                       "}\n";
        output_file.close();
    }

public:
    explicit LLVM_Compiler(Program *program, const std::filesystem::path &file_path) : program(program) {
        output_file.open(file_path);
        if (!output_file.is_open()) {
            std::cerr << "can't open output file" << std::endl;
            exit(1);
        }
    }

    void compile() {
        init_common();
        auto var_init_visitor = new LLVM_Var_Init_Visitor;
        program->accept(var_init_visitor);
        variables = var_init_visitor->variables;

        for (const auto &variable: variables) {
            output_file << "\t%" + variable + " = alloca i32\n";
        }

        auto compile_visitor = new LLVM_Compile_Visitor(output_file, variables);
        program->accept(compile_visitor);
        delete (var_init_visitor);
        delete (compile_visitor);
        close_common();
    }
};