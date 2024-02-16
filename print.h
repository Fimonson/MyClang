#pragma once

#include <string_view>
#include <llvm/Support/raw_ostream.h>

enum class Entity { kVariable, kField, kType, kConst, kFunction };

inline std::string Str(Entity entity) {
    switch (entity) {
        case Entity::kVariable:
            return "variable";
        case Entity::kField:
            return "field";
        case Entity::kType:
            return "type";
        case Entity::kConst:
            return "const";
        case Entity::kFunction:
            return "function";
        default:
            throw std::runtime_error{"Bad entity"};
    }
}

inline void PrintStatistics(std::string_view filename, int bad_names, int mistakes,
                            llvm::raw_ostream& os = llvm::outs()) {
    os << "===== Processing " << filename << " =====\n";
    os << "Bad names found: " << bad_names << "\n";
    os << "Possible mistakes: " << mistakes << "\n\n";
}

inline void BadName(Entity entity, std::string_view name, std::string_view filename, int line,
                    llvm::raw_ostream& os = llvm::outs()) {
    os << "Entity's name \"" << name << "\" does not meet the requirements (" << Str(entity)
       << ")\n";
    os << "In " << filename << " at line " << line << "\n\n";
}

inline void Mistake(std::string_view name, std::string_view wrong_word, std::string_view ok_word,
                    std::string_view filename, int line, llvm::raw_ostream& os = llvm::outs()) {
    os << "Probably mistake in variable's \"" << name << "\" name.\n";
    os << "Consider using \"" << ok_word << "\" instead of \"" << wrong_word << "\"\n";
    os << "In " << filename << " at line " << line << "\n\n";
}
