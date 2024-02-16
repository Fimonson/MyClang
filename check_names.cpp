#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <clang/Frontend/FrontendActions.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include <regex>
#include <string>
#include <vector>
#include <fstream>
#include "print.h"

struct BadNames {
    Entity entity;
    std::string name;
    std::string file_name;
    unsigned int number;
    bool bad_mistake = true;
    std::string good_name = "";
    std::string wrong_name = "";
};

class MyPrint {
public:
    void SetFileName(const std::string& filename) {
        // bad_names_.clear();
        if (filename_.empty()) {
            filename_ = filename;
            return;
        }
        if (filename != filename_) {
            Print();
            filename_ = filename;
        }
    }

    void SetBadNames(const BadNames& names) {
        bad_names_.push_back(names);
        if (names.bad_mistake) {
            ++bad_num_;
        }
    }

    void Print() {
        std::string true_file_name = filename_.substr(filename_.find_last_of('/') + 1);
        PrintStatistics(true_file_name, bad_num_, bad_names_.size() - bad_num_);
        for (auto item : bad_names_) {
            true_file_name = item.file_name.substr(item.file_name.find_last_of('/') + 1);
            if (item.bad_mistake) {
                BadName(item.entity, item.name, true_file_name, item.number);
            } else {
                Mistake(item.name, item.wrong_name, item.good_name, true_file_name, item.number);
            }
        }
        bad_num_ = 0;
        bad_names_.clear();
        filename_.clear();
    }

private:
    std::string filename_;             // NOLINT
    std::vector<BadNames> bad_names_;  // NOLINT
    int bad_num_ = 0;                  // NOLINT
};

MyPrint printer;
std::vector<std::string> my_dict;

int LevDistance(const std::string& wrong_name, const std::string& good_name, size_t i, size_t j,
                std::vector<std::vector<int>> matrix) {
    if (i == 0 && j == 0) {
        return 0;
    } else if (i == 0 && j > 0) {
        return j;
    } else if (j == 0 && i > 0) {
        return i;
    } else {
        int m = (tolower(wrong_name[i - 1]) == good_name[j - 1]) ? 0 : 1;
        return std::min(matrix[i][j - 1] + 1,
                        std::min(matrix[i - 1][j] + 1, matrix[i - 1][j - 1] + m));
    }
}

int CalcLevDistance(const std::string& wrong_name, const std::string& good_name) {
    size_t n = wrong_name.size();
    size_t m = good_name.size();
    std::vector<std::vector<int>> matrix(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = 0; i < n + 1; ++i) {
        for (size_t j = 0; j < m + 1; ++j) {
            matrix[i][j] = LevDistance(wrong_name, good_name, i, j, matrix);
        }
    }
    return matrix[n][m];
}

void CalcMistake(const std::string& string, const std::string& filename,
                 const unsigned int& number) {
    std::vector<std::string> substr;
    std::string token;
    for (size_t i = 0; i < string.size(); ++i) {
        if (string[i] == '_') {
            substr.push_back(token);
            token.clear();
            continue;
        }
        if (string[i] >= 'A' && string[i] <= 'Z') {
            if (!token.empty()) {
                substr.push_back(token);
                token.clear();
                token.push_back(string[i]);
                continue;
            }
        }
        token.push_back(string[i]);
    }
    substr.push_back(token);
    for (auto item : substr) {
        if (item.size() <= 3) {
            continue;
        }
        int min = 4;
        int min_ind = 0;
        for (size_t i = 0; i < my_dict.size(); ++i) {
            if (CalcLevDistance(item, my_dict[i]) < min) {
                min = CalcLevDistance(item, my_dict[i]);
                min_ind = i;
            }
        }
        if (min < 4 && min > 0) {
            printer.SetBadNames(
                {Entity::kVariable, string, filename, number, false, my_dict[min_ind], item});
        }
    }
}

using namespace clang::ast_matchers;  // NOLINT
// var
class CallbackForVarDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* var_decl = result.Nodes.getNodeAs<clang::VarDecl>("var");
        if (!var_decl) {
            return;
        }
        auto loc = result.Context->getFullLoc(var_decl->getLocation());
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (var_decl->getNameAsString().empty()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        auto var_loc = result.SourceManager->isMacroBodyExpansion(loc)
                           ? result.SourceManager->getImmediateMacroCallerLoc(loc)
                           : var_decl->getLocation();
        if (var_decl->isConstexpr() || var_decl->getType().isConstQualified()) {  // const
            if (!std::regex_match(var_decl->getNameAsString(),
                                  std::regex(R"(k([A-Z]{1,}[a-z]+)+)"))) {
                printer.SetBadNames({Entity::kConst, var_decl->getNameAsString(),
                                     std::string(result.SourceManager->getFilename(var_loc)),
                                     result.SourceManager->getSpellingLineNumber(var_loc)});
                return;
            }
        } else if (var_decl->isCXXClassMember()) {
            if (var_decl->getAccess() != clang::AccessSpecifier::AS_public) {  // private
                if (!std::regex_match(var_decl->getNameAsString(),
                                      std::regex(R"(([a-z]+\_[a-z]+\_)|([a-z]+\_))"))) {
                    printer.SetBadNames({Entity::kField, var_decl->getNameAsString(),
                                         std::string(result.SourceManager->getFilename(var_loc)),
                                         result.SourceManager->getSpellingLineNumber(var_loc)});
                    return;
                }
            } else {  // no private
                if (!std::regex_match(var_decl->getNameAsString(),
                                      std::regex(R"([a-z]+(_[a-z]+)*)"))) {
                    printer.SetBadNames({Entity::kVariable, var_decl->getNameAsString(),
                                         std::string(result.SourceManager->getFilename(var_loc)),
                                         result.SourceManager->getSpellingLineNumber(var_loc)});
                    return;
                }
            }
        } else {  // no const
            if (!std::regex_match(var_decl->getNameAsString(), std::regex(R"([a-z]+(_[a-z]+)*)"))) {
                printer.SetBadNames({Entity::kVariable, var_decl->getNameAsString(),
                                     std::string(result.SourceManager->getFilename(var_loc)),
                                     result.SourceManager->getSpellingLineNumber(var_loc)});
                return;
            }
        }
        if (var_decl->getNameAsString().size() > 3) {
            CalcMistake(var_decl->getNameAsString(),
                        std::string(result.SourceManager->getFilename(var_loc)),
                        result.SourceManager->getSpellingLineNumber(var_loc));
        }
    }
};
// field
class CallbackForFieldDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* field_decl = result.Nodes.getNodeAs<clang::FieldDecl>("field");
        if (!field_decl) {
            return;
        }
        auto field_loc = field_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(field_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        if (field_decl->getType().isConstQualified()) {  // const
            if (!std::regex_match(field_decl->getNameAsString(),
                                  std::regex(R"(k([A-Z]{1,}[a-z]+)+)"))) {
                printer.SetBadNames({Entity::kConst, field_decl->getNameAsString(),
                                     std::string(result.SourceManager->getFilename(field_loc)),
                                     result.SourceManager->getSpellingLineNumber(field_loc)});
                return;
            }
        } else {                                                                 // no const
            if (field_decl->getAccess() != clang::AccessSpecifier::AS_public) {  // private
                if (!std::regex_match(field_decl->getNameAsString(),
                                      std::regex(R"(([a-z]+\_[a-z]+\_)|([a-z]+\_))"))) {
                    printer.SetBadNames({Entity::kField, field_decl->getNameAsString(),
                                         std::string(result.SourceManager->getFilename(field_loc)),
                                         result.SourceManager->getSpellingLineNumber(field_loc)});
                    return;
                }
            } else {  // no private
                if (!std::regex_match(field_decl->getNameAsString(),
                                      std::regex(R"([a-z]+(_[a-z]+)*)"))) {
                    printer.SetBadNames({Entity::kVariable, field_decl->getNameAsString(),
                                         std::string(result.SourceManager->getFilename(field_loc)),
                                         result.SourceManager->getSpellingLineNumber(field_loc)});
                    return;
                }
            }
        }
        if (field_decl->getNameAsString().size() > 3) {
            CalcMistake(field_decl->getNameAsString(),
                        std::string(result.SourceManager->getFilename(field_loc)),
                        result.SourceManager->getSpellingLineNumber(field_loc));
        }
    }
};
// function
class CallbackForFunctionDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* function_decl = result.Nodes.getNodeAs<clang::FunctionDecl>("function");
        if (!function_decl) {
            return;
        }
        auto function_loc = function_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(function_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (function_decl->isMain() || function_decl->isOverloadedOperator()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        std::string name = function_decl->getNameAsString();
        if (function_decl->isTemplated()) {
            name = name.substr(0, name.find('<'));
        }
        if (name == "run") {
            return;
        }
        if (!std::regex_match(name, std::regex(R"(^(([A-Z]{3,})?[A-Z][a-z]+)+([A-Z]{3,})?$)"))) {
            printer.SetBadNames({Entity::kFunction, name,
                                 std::string(result.SourceManager->getFilename(function_loc)),
                                 result.SourceManager->getSpellingLineNumber(function_loc)});
        } else {
            if (function_decl->getNameAsString().size() > 3) {
                CalcMistake(function_decl->getNameAsString(),
                            std::string(result.SourceManager->getFilename(function_loc)),
                            result.SourceManager->getSpellingLineNumber(function_loc));
            }
        }
    }
};

// class or struct
class CallbackForRecordDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* record_decl = result.Nodes.getNodeAs<clang::CXXRecordDecl>("record");
        if (!record_decl) {
            return;
        }
        auto record_loc = record_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(record_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (record_decl->getNameAsString().empty()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        if (!std::regex_match(record_decl->getNameAsString(),
                              std::regex(R"(^(([A-Z]{3,})?[A-Z][a-z]+)+([A-Z]{3,})?$)"))) {
            printer.SetBadNames({Entity::kType, record_decl->getNameAsString(),
                                 std::string(result.SourceManager->getFilename(record_loc)),
                                 result.SourceManager->getSpellingLineNumber(record_loc)});
        } else {
            if (record_decl->getNameAsString().size() > 3) {
                CalcMistake(record_decl->getNameAsString(),
                            std::string(result.SourceManager->getFilename(record_loc)),
                            result.SourceManager->getSpellingLineNumber(record_loc));
            }
        }
    }
};
// enum
class CallbackForEnumDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* enum_decl = result.Nodes.getNodeAs<clang::EnumDecl>("enum");
        if (!enum_decl) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        auto enum_loc = enum_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(enum_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (enum_decl->getNameAsString().empty()) {
            return;
        }
        if (!std::regex_match(enum_decl->getNameAsString(),
                              std::regex(R"(^(([A-Z]{3,})?[A-Z][a-z]+)+([A-Z]{3,})?$)"))) {
            printer.SetBadNames({Entity::kType, enum_decl->getNameAsString(),
                                 std::string(result.SourceManager->getFilename(enum_loc)),
                                 result.SourceManager->getSpellingLineNumber(enum_loc)});
        } else {
            if (enum_decl->getNameAsString().size() > 3) {
                CalcMistake(enum_decl->getNameAsString(),
                            std::string(result.SourceManager->getFilename(enum_loc)),
                            result.SourceManager->getSpellingLineNumber(enum_loc));
            }
        }
    }
};
// TypeAlias
class CallbackForTypeAliasDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* alias_decl = result.Nodes.getNodeAs<clang::TypeAliasDecl>("alias");
        if (!alias_decl) {
            return;
        }
        auto alias_loc = alias_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(alias_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (alias_decl->getNameAsString().empty()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        if (!std::regex_match(alias_decl->getNameAsString(),
                              std::regex(R"(^(([A-Z]{3,})?[A-Z][a-z]+)+([A-Z]{3,})?$)"))) {
            printer.SetBadNames({Entity::kType, alias_decl->getNameAsString(),
                                 std::string(result.SourceManager->getFilename(alias_loc)),
                                 result.SourceManager->getSpellingLineNumber(alias_loc)});
        } else {
            if (alias_decl->getNameAsString().size() > 3) {
                CalcMistake(alias_decl->getNameAsString(),
                            std::string(result.SourceManager->getFilename(alias_loc)),
                            result.SourceManager->getSpellingLineNumber(alias_loc));
            }
        }
    }
};

class CallbackForTypeDefDecl : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        auto* type_decl = result.Nodes.getNodeAs<clang::TypedefDecl>("decl");
        if (!type_decl) {
            return;
        }
        auto type_loc = type_decl->getBeginLoc();
        auto loc = result.Context->getFullLoc(type_loc);
        if (!loc.isValid() || loc.isInSystemHeader()) {
            return;
        }
        if (type_decl->getNameAsString().empty()) {
            return;
        }
        auto parent_filename =
            result.SourceManager->getFileEntryRefForID(result.SourceManager->getMainFileID())
                .getValue()
                .getName()
                .data();
        printer.SetFileName(parent_filename);
        if (!std::regex_match(type_decl->getNameAsString(),
                              std::regex(R"(^(([A-Z]{3,})?[A-Z][a-z]+)+([A-Z]{3,})?$)"))) {
            printer.SetBadNames({Entity::kType, type_decl->getNameAsString(),
                                 std::string(result.SourceManager->getFilename(type_loc)),
                                 result.SourceManager->getSpellingLineNumber(type_loc)});
        } else {
            if (type_decl->getNameAsString().size() > 3) {
                CalcMistake(type_decl->getNameAsString(),
                            std::string(result.SourceManager->getFilename(type_loc)),
                            result.SourceManager->getSpellingLineNumber(type_loc));
            }
        }
    }
};

int main(int argc, const char** argv) {
    llvm::cl::OptionCategory category{"my category"};

    llvm::cl::opt<std::string> dict{"dict", llvm::cl::cat{category}};

    auto expected_parser = clang::tooling::CommonOptionsParser::create(argc, argv, category);

    if (!expected_parser) {
        llvm::errs() << expected_parser.takeError() << '\n';
        return 1;
    }

    auto& parser = *expected_parser;
    clang::tooling::ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};

    if (!dict.empty()) {
        std::fstream file_stream;
        file_stream.open(dict.getValue());
        try {
            if (file_stream.is_open()) {
                std::string cur_word;
                while (file_stream >> cur_word) {
                    my_dict.push_back(cur_word);
                }
            }
        } catch (...) {
            file_stream.close();
            std::rethrow_exception(std::current_exception());
        }
        file_stream.close();
    }

    clang::ast_matchers::MatchFinder finder;
    CallbackForVarDecl callback_var;
    CallbackForFieldDecl callback_field;
    CallbackForFunctionDecl callback_function;
    CallbackForRecordDecl callback_record;
    CallbackForEnumDecl callback_enum;
    CallbackForTypeAliasDecl callback_alias;
    CallbackForTypeDefDecl callback_decl;
    auto matcher_var = varDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("var");
    auto matcher_field = fieldDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("field");
    auto matcher_function =
        functionDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("function");
    auto matcher_record =
        cxxRecordDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("record");
    auto matcher_enum = enumDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("enum");
    auto matcher_alias = typeAliasDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("alias");
    auto matcher_decl = typedefDecl(unless(anyOf(isImplicit(), isInstantiated()))).bind("decl");
    finder.addMatcher(matcher_var, &callback_var);
    finder.addMatcher(matcher_field, &callback_field);
    finder.addMatcher(matcher_function, &callback_function);
    finder.addMatcher(matcher_record, &callback_record);
    finder.addMatcher(matcher_enum, &callback_enum);
    finder.addMatcher(matcher_alias, &callback_alias);
    finder.addMatcher(matcher_decl, &callback_decl);
    auto factory = clang::tooling::newFrontendActionFactory(&finder);
    tool.run(factory.get());
    printer.Print();
    return 0;
}