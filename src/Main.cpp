/* File generated by the BNF Converter (bnfc 2.9.3) (Test.C) and the modified by me */

#include <cstdio>
#include <string>
#include <iostream>
#include "Parser.H"
#include "Printer.H"
#include "Absyn.H"
#include "ParserError.H"
#include "LLVM_Compiler.cpp"

void usage() {
    printf("usage: Call with one of the following argument combinations:\n");
    printf("\t--help\t\tDisplay this help message.\n");
    printf("\t(no arguments)\tParse stdin verbosely.\n");
    printf("\t(files)\t\tParse content of files verbosely.\n");
    printf("\t-s (files)\tSilent mode. Parse content of files silently.\n");
}

int main(int argc, char **argv) {
    FILE *input;
    int quiet = 0;
    char *filename = NULL;

    if (argc > 1) {
        if (strcmp(argv[1], "-s") == 0) {
            quiet = 1;
            if (argc > 2) {
                filename = argv[2];
            } else {
                input = stdin;
            }
        } else {
            filename = argv[1];
        }
    }

    if (filename) {
        input = fopen(filename, "r");
        if (!input) {
            usage();
            exit(1);
        }
    } else input = stdin;
    /* The default entry point is used. For other options see Parser.H */
    Program *parse_tree = NULL;
    try {
        parse_tree = pProgram(input);
    } catch (parse_error &e) {
        std::cerr << "Parse error on line " << e.getLine() << "\n";
    }
    if (parse_tree) {
        printf("\nParse Successful!\n");
        if (!quiet) {
            printf("\n[Abstract Syntax]\n");
            ShowAbsyn *s = new ShowAbsyn();
            printf("%s\n\n", s->show(parse_tree));
            printf("[Linearized Tree]\n");
            PrintAbsyn *p = new PrintAbsyn();
            printf("%s\n\n", p->print(parse_tree));
        }
        auto compiler = new LLVM_Compiler(parse_tree, "todofilename.ll"); // TODO filename from input arg, for scripts
        compiler->compile();

        delete (parse_tree);
        return 0;
    }
    return 1;
}

