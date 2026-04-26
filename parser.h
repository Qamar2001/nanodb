// parser.h - SQL-like parser.
// Handles:  SELECT <cols|*> FROM <tbl> [WHERE <expr>]
//           INSERT INTO <tbl> VALUES (v1, v2, ...)
//           SELECT ... FROM t1 JOIN t2 [JOIN t3] [WHERE <expr>]
// WHERE expressions support:
//   identifiers (column names), int/float literals, string literals "..."
//   binary ops: ==  !=  <  >  <=  >=  AND  OR  +  -  *  /  %
//   parentheses
//
// Parsing strategy:
//   1. tokenize
//   2. extract the WHERE expression and convert infix -> postfix (Shunting-Yard)
//   3. at evaluation time, use a custom Stack over DataType* to reduce the postfix
//
// Stacks are always our Stack<T> (no STL).
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include "data_types.h"

enum TokenType {
    TK_EOF,
    TK_IDENT,         // column name or keyword
    TK_NUMBER,        // int or float literal
    TK_STRING,        // "..." literal
    TK_STAR,          // *
    TK_COMMA,
    TK_LPAREN,
    TK_RPAREN,
    TK_OP             // any operator: + - * / % == != < > <= >= AND OR
};

struct Token {
    TokenType type;
    std::string text;   // raw token text (or operator)
    // parsed literal values (valid when type == TK_NUMBER)
    double numVal;
    bool isFloat;
    Token() : type(TK_EOF), numVal(0), isFloat(false) {}
};

// --- token stream as a raw growable array ---
class TokenStream {
public:
    Token* tokens;
    int count;
    int capacity;
    TokenStream();
    ~TokenStream();
    TokenStream(const TokenStream&) = delete;
    TokenStream& operator=(const TokenStream&) = delete;
    void push(const Token& t);
};

// Tokenizer. Returns owned TokenStream* - caller deletes.
TokenStream* tokenize(const std::string& sql);

// Shunting-yard. Copies tokens in [start, end) into output postfix stream.
// Returns new TokenStream*. Caller deletes.
TokenStream* infixToPostfix(const TokenStream* in, int start, int end);

// Pretty-print a token stream (for logging).
std::string printTokens(const TokenStream* ts, int start, int end);

// Statement types the parser produces.
enum StmtKind { STMT_SELECT, STMT_INSERT, STMT_HELP, STMT_UNKNOWN };

struct Statement {
    StmtKind kind;
    // common
    bool isAdmin;                 // prefixed with "ADMIN "

    // SELECT
    std::string* selectCols;      // array of column names (or "*" for single)
    int selectColCount;
    std::string* fromTables;      // one or more tables (joins)
    int fromTableCount;
    TokenStream* wherePostfix;    // nullptr if no WHERE
    std::string whereInfixStr;    // for logging

    // INSERT
    std::string insertTable;
    std::string* insertVals;      // raw string values, parsed later
    int insertValCount;

    Statement();
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
};

// Parse a full SQL-like statement. Returns owned Statement*. Caller deletes.
Statement* parseStatement(const std::string& sql);

#endif
