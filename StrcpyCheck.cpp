//===--- StrcpyCheck.cpp - clang-tidy--------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "StrcpyCheck.h"
#include "../utils/LexerUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Tooling/FixIt.h"
#include <iostream>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {

/* TODO:
 * - remove .c_str() in source when it is std::string
 * - replace strncpy by my_safe_copy when the size is already known
 */
StrcpyCheck::StrcpyCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      Inserter(Options.getLocalOrGlobal("IncludeStyle",
                                        utils::IncludeSorter::IS_LLVM)) {}

void StrcpyCheck::registerPPCallbacks(const SourceManager &SM,
                                      Preprocessor *PP,
                                      Preprocessor *ModuleExpanderPP) {
  Inserter.registerPreprocessor(PP);
}

void StrcpyCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "IncludeStyle", Inserter.getStyle());
}

void StrcpyCheck::registerMatchers(MatchFinder *finder) {

  const auto StringDecl = type(hasUnqualifiedDesugaredType(recordType(
      hasDeclaration(cxxRecordDecl(hasName("::std::basic_string"))))));
  const auto StringExpr =
      expr(anyOf(hasType(StringDecl), hasType(qualType(pointsTo(StringDecl)))));

  // Match a call to the string 'c_str()' method.
  const auto StringCStrCallExpr =
      cxxMemberCallExpr(on(StringExpr.bind("std_string_variable")),
                        callee(memberExpr().bind("member")),
                        callee(cxxMethodDecl(hasAnyName("c_str", "data"))))
          .bind("call_c_str");

  // strcpy
  finder->addMatcher(
      callExpr(callee(functionDecl(hasName("strcpy"))),
               hasArgument(0, expr(hasType(arrayType())).bind("destination")),
               hasArgument(1, anyOf(StringCStrCallExpr, expr().bind("source"))))
          .bind("call_strcpy"),
      this);
}

void StrcpyCheck::check(const MatchFinder::MatchResult &Result) {
  checkStrcpy(Result);
}

void StrcpyCheck::checkStrcpy(const MatchFinder::MatchResult &Result) {
  const auto *MatchedStrcpyCall =
      Result.Nodes.getNodeAs<CallExpr>("call_strcpy");

  auto Diag =
      diag(MatchedStrcpyCall->getExprLoc(),
           "Replace strcpy by my_safe_copy", DiagnosticIDs::Warning);

  const llvm::StringRef DestinationArgText = getDestinationText(Result);
  const llvm::StringRef SourceArgText = getSourceText(Result);

  std::string ReplacementText = "my_safe_copy(" +
                                DestinationArgText.str() + ", " +
                                SourceArgText.str() + ")";

  Diag << FixItHint::CreateReplacement(
      CharSourceRange::getTokenRange(MatchedStrcpyCall->getBeginLoc(),
                                     MatchedStrcpyCall->getEndLoc()),
      ReplacementText);

  if (auto Fix = Inserter.createIncludeInsertion(
          Result.SourceManager->getMainFileID(), "\"safe_strcpy.hpp\"")) {
    Diag << *Fix;
  }
}

llvm::StringRef
StrcpyCheck::getDestinationText(const MatchFinder::MatchResult &Result) {
  const auto *DestinationArg = Result.Nodes.getNodeAs<Expr>("destination");

  return getArgumentText(DestinationArg, *Result.Context, *Result.SourceManager);
}

llvm::StringRef
StrcpyCheck::getSourceText(const MatchFinder::MatchResult &Result) {
  const auto *SourceArg = Result.Nodes.getNodeAs<Expr>("source");
  const auto *SourceArgCStr = Result.Nodes.getNodeAs<Expr>("call_c_str");

  if (SourceArgCStr) {
    return getArgumentText(Result.Nodes.getNodeAs<Expr>("std_string_variable"),
                           *Result.Context, *Result.SourceManager);
  } else {
    return getArgumentText(SourceArg, *Result.Context, *Result.SourceManager);
  }
}

llvm::StringRef StrcpyCheck::getArgumentText(const Expr *Argument,
                                                 ASTContext &Context,
                                                 SourceManager &SourceManager) {
  llvm::StringRef ArgText;
  if (!Argument->getExprLoc().isMacroID()) {
    ArgText = tooling::fixit::getText(*Argument, Context);
  } else {
    const CharSourceRange expansionRange =
        SourceManager.getImmediateExpansionRange(Argument->getBeginLoc());

    ArgText = Lexer::getSourceText(expansionRange, Context.getSourceManager(),
                                   Context.getLangOpts());
  }

  return ArgText;
}

} // namespace tidy
} // namespace clang