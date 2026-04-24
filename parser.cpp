// parser.cpp
#include "parser.h"
#include "stack.h"
#include "logger.h"
#include <cctype>
#include <cstdlib>

// --------- TokenStream ---------
TokenStream::TokenStream() {
    capacity = 32; count = 0;
    tokens = new Token[capacity];
}
TokenStream::~TokenStream() { delete[] tokens; }
void TokenStream::push(const Token& t) {
    if (count == capacity) {
        int nc = capacity * 2;
        Token* nt = new Token[nc];
        for (int i = 0; i < count; i++) nt[i] = tokens[i];
        delete[] tokens; tokens = nt; capacity = nc;
    }
    tokens[count++] = t;
}

// --------- Tokenizer ---------
static bool isIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool isIdentCont(char c)  { return isalnum((unsigned char)c) || c == '_'; }

// uppercase copy of a string
static std::string upper(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++) r[i] = (char)toupper((unsigned char)r[i]);
    return r;
}

TokenStream* tokenize(const std::string& sql) {
    TokenStream* ts = new TokenStream();
    size_t i = 0, n = sql.size();
    while (i < n) {
        char c = sql[i];
        if (isspace((unsigned char)c)) { i++; continue; }

        Token t;

        // string literal
        if (c == '"' || c == '\'') {
            char q = c; i++;
            std::string s;
            while (i < n && sql[i] != q) { s += sql[i]; i++; }
            if (i < n) i++;      // consume closing quote
            t.type = TK_STRING; t.text = s;
            ts->push(t); continue;
        }

        // number
        if (isdigit((unsigned char)c) || (c == '.' && i+1 < n && isdigit((unsigned char)sql[i+1]))) {
            std::string num;
            bool sawDot = false;
            while (i < n && (isdigit((unsigned char)sql[i]) || sql[i] == '.')) {
                if (sql[i] == '.') { if (sawDot) break; sawDot = true; }
                num += sql[i]; i++;
            }
            t.type = TK_NUMBER; t.text = num;
            t.numVal = atof(num.c_str());
            t.isFloat = sawDot;
            ts->push(t); continue;
        }

        // identifier / keyword
        if (isIdentStart(c)) {
            std::string w;
            while (i < n && isIdentCont(sql[i])) { w += sql[i]; i++; }
            // AND / OR become operators
            std::string u = upper(w);
            if (u == "AND" || u == "OR") {
                t.type = TK_OP; t.text = u;
            } else {
                t.type = TK_IDENT; t.text = w;
            }
            ts->push(t); continue;
        }

        // punctuation / operators
        if (c == '(') { t.type = TK_LPAREN; t.text = "("; ts->push(t); i++; continue; }
        if (c == ')') { t.type = TK_RPAREN; t.text = ")"; ts->push(t); i++; continue; }
        if (c == ',') { t.type = TK_COMMA;  t.text = ","; ts->push(t); i++; continue; }
        if (c == '*') {
            // heuristic: * is multiplication only if the previous token looks
            // like a value. After SELECT/WHERE/FROM etc, it's the star column.
            bool prevIsValue = false;
            if (ts->count > 0) {
                const Token& prev = ts->tokens[ts->count-1];
                TokenType pt = prev.type;
                if (pt == TK_NUMBER || pt == TK_RPAREN || pt == TK_STRING) {
                    prevIsValue = true;
                } else if (pt == TK_IDENT) {
                    std::string up = upper(prev.text);
                    // SQL keywords do NOT act as values for this purpose
                    bool isKw = (up == "SELECT" || up == "FROM" || up == "WHERE" ||
                                 up == "AND"    || up == "OR"   || up == "JOIN"  ||
                                 up == "INSERT" || up == "INTO" || up == "VALUES");
                    prevIsValue = !isKw;
                }
            }
            if (prevIsValue) { t.type = TK_OP; t.text = "*"; }
            else             { t.type = TK_STAR; t.text = "*"; }
            ts->push(t); i++; continue;
        }
        if (c == '+' || c == '-' || c == '/' || c == '%') {
            t.type = TK_OP; t.text = std::string(1, c); ts->push(t); i++; continue;
        }
        if (c == '=' ) {
            if (i+1 < n && sql[i+1] == '=') { t.type = TK_OP; t.text = "=="; ts->push(t); i += 2; continue; }
            t.type = TK_OP; t.text = "=="; ts->push(t); i++; continue;   // treat single = as ==
        }
        if (c == '!' && i+1 < n && sql[i+1] == '=') { t.type = TK_OP; t.text = "!="; ts->push(t); i += 2; continue; }
        if (c == '<') {
            if (i+1 < n && sql[i+1] == '=') { t.type = TK_OP; t.text = "<="; ts->push(t); i += 2; continue; }
            t.type = TK_OP; t.text = "<"; ts->push(t); i++; continue;
        }
        if (c == '>') {
            if (i+1 < n && sql[i+1] == '=') { t.type = TK_OP; t.text = ">="; ts->push(t); i += 2; continue; }
            t.type = TK_OP; t.text = ">"; ts->push(t); i++; continue;
        }
        // unknown char - skip
        i++;
    }
    return ts;
}

// --------- Operator precedence (higher = binds tighter) ---------
static int prec(const std::string& op) {
    if (op == "OR")  return 1;
    if (op == "AND") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
    return 0;
}

// all our binary ops are left-associative.
TokenStream* infixToPostfix(const TokenStream* in, int start, int end) {
    TokenStream* out = new TokenStream();
    Stack<int> opStack(32);    // store indices into 'in'
    for (int i = start; i < end; i++) {
        const Token& t = in->tokens[i];
        if (t.type == TK_NUMBER || t.type == TK_STRING || t.type == TK_IDENT) {
            out->push(t);
        } else if (t.type == TK_LPAREN) {
            opStack.push(i);
        } else if (t.type == TK_RPAREN) {
            // pop until matching LPAREN
            while (!opStack.empty()) {
                int oi = opStack.top();
                if (in->tokens[oi].type == TK_LPAREN) { opStack.pop(); break; }
                out->push(in->tokens[oi]);
                opStack.pop();
            }
        } else if (t.type == TK_OP) {
            while (!opStack.empty()) {
                int oi = opStack.top();
                const Token& tt = in->tokens[oi];
                if (tt.type == TK_LPAREN) break;
                if (prec(tt.text) >= prec(t.text)) {
                    out->push(tt);
                    opStack.pop();
                } else break;
            }
            opStack.push(i);
        }
    }
    while (!opStack.empty()) {
        int oi = opStack.pop();
        if (in->tokens[oi].type == TK_LPAREN) continue;
        out->push(in->tokens[oi]);
    }
    return out;
}

std::string printTokens(const TokenStream* ts, int start, int end) {
    std::string out;
    for (int i = start; i < end; i++) {
        if (i > start) out += " ";
        out += ts->tokens[i].text;
    }
    return out;
}

// --------- Statement ---------
Statement::Statement()
    : kind(STMT_UNKNOWN), isAdmin(false),
      selectCols(nullptr), selectColCount(0),
      fromTables(nullptr), fromTableCount(0),
      wherePostfix(nullptr),
      insertVals(nullptr), insertValCount(0) {}

Statement::~Statement() {
    delete[] selectCols;
    delete[] fromTables;
    if (wherePostfix) delete wherePostfix;
    delete[] insertVals;
}

// Helper: find token by uppercased IDENT text, starting from 'from'.
static int findKeyword(const TokenStream* ts, const std::string& kw, int from = 0) {
    for (int i = from; i < ts->count; i++) {
        if (ts->tokens[i].type == TK_IDENT && upper(ts->tokens[i].text) == kw) return i;
    }
    return -1;
}

Statement* parseStatement(const std::string& sqlIn) {
    std::string sql = sqlIn;
    // trim
    while (!sql.empty() && isspace((unsigned char)sql.back())) sql.pop_back();
    while (!sql.empty() && isspace((unsigned char)sql.front())) sql.erase(sql.begin());

    Statement* stmt = new Statement();

    // admin prefix
    if (sql.size() >= 6 && upper(sql.substr(0, 6)) == "ADMIN ") {
        stmt->isAdmin = true;
        sql = sql.substr(6);
    }

    TokenStream* ts = tokenize(sql);
    if (ts->count == 0) { delete ts; return stmt; }

    std::string first = (ts->tokens[0].type == TK_IDENT) ? upper(ts->tokens[0].text) : "";

    if (first == "SELECT") {
        stmt->kind = STMT_SELECT;
        int fromIdx = findKeyword(ts, "FROM", 1);
        int whereIdx = (fromIdx >= 0) ? findKeyword(ts, "WHERE", fromIdx + 1) : -1;

        // columns: tokens 1..fromIdx-1 (idents or star, comma-separated)
        int colEnd = (fromIdx >= 0) ? fromIdx : ts->count;
        // count commas+1
        int cc = 0;
        for (int i = 1; i < colEnd; i++) {
            if (ts->tokens[i].type == TK_IDENT || ts->tokens[i].type == TK_STAR) cc++;
        }
        stmt->selectCols = new std::string[cc > 0 ? cc : 1];
        stmt->selectColCount = 0;
        for (int i = 1; i < colEnd; i++) {
            if (ts->tokens[i].type == TK_IDENT || ts->tokens[i].type == TK_STAR)
                stmt->selectCols[stmt->selectColCount++] = ts->tokens[i].text;
        }

        // FROM ... tables: ident ( "JOIN" ident )* up to WHERE or end
        if (fromIdx >= 0) {
            int tEnd = (whereIdx >= 0) ? whereIdx : ts->count;
            int tc = 0;
            for (int i = fromIdx + 1; i < tEnd; i++) {
                if (ts->tokens[i].type == TK_IDENT && upper(ts->tokens[i].text) != "JOIN")
                    tc++;
            }
            stmt->fromTables = new std::string[tc > 0 ? tc : 1];
            stmt->fromTableCount = 0;
            for (int i = fromIdx + 1; i < tEnd; i++) {
                if (ts->tokens[i].type == TK_IDENT && upper(ts->tokens[i].text) != "JOIN")
                    stmt->fromTables[stmt->fromTableCount++] = ts->tokens[i].text;
            }
        }

        // WHERE ...
        if (whereIdx >= 0) {
            stmt->whereInfixStr = printTokens(ts, whereIdx + 1, ts->count);
            stmt->wherePostfix = infixToPostfix(ts, whereIdx + 1, ts->count);
            if (g_logger) {
                std::string post = printTokens(stmt->wherePostfix, 0, stmt->wherePostfix->count);
                g_logger->log("Infix \"" + stmt->whereInfixStr + "\" converted to Postfix \"" + post + "\"");
            }
        }

    } else if (first == "INSERT") {
        stmt->kind = STMT_INSERT;
        // INSERT INTO <tbl> VALUES ( v1, v2, ... )
        int intoIdx = findKeyword(ts, "INTO", 1);
        int valIdx  = findKeyword(ts, "VALUES", (intoIdx >= 0 ? intoIdx + 1 : 1));
        if (intoIdx + 1 < ts->count && ts->tokens[intoIdx + 1].type == TK_IDENT) {
            stmt->insertTable = ts->tokens[intoIdx + 1].text;
        }
        // collect value tokens between '(' ... ')' after VALUES
        int lp = -1, rp = -1;
        for (int i = valIdx + 1; i < ts->count; i++) {
            if (ts->tokens[i].type == TK_LPAREN) { lp = i; break; }
        }
        if (lp >= 0) {
            for (int i = lp + 1; i < ts->count; i++) {
                if (ts->tokens[i].type == TK_RPAREN) { rp = i; break; }
            }
        }
        if (lp >= 0 && rp >= 0) {
            // split by commas
            int vc = 0;
            for (int i = lp + 1; i < rp; i++) {
                if (ts->tokens[i].type != TK_COMMA) vc++;
            }
            stmt->insertVals = new std::string[vc > 0 ? vc : 1];
            stmt->insertValCount = 0;
            for (int i = lp + 1; i < rp; i++) {
                if (ts->tokens[i].type != TK_COMMA)
                    stmt->insertVals[stmt->insertValCount++] = ts->tokens[i].text;
            }
        }
    }

    delete ts;
    return stmt;
}
