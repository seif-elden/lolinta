#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <sstream>

using namespace std;

// Violation Struct
struct Violation {
    string message;
    int line;
};

// Tokenizer for Verilog
class VerilogParser {
private:
    vector<string> lines;

public:
    explicit VerilogParser(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Unable to open file " << filename << endl;
            exit(EXIT_FAILURE);
        }
        string line;
        while (getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    }

    const vector<string>& getLines() const {
        return lines;
    }
};

// Static Checker Engine
class StaticChecker {
private:
    vector<string> lines;
    vector<Violation> violations;

    // Initialization Checks
    void checkUninitializedRegisters() {
        regex regPattern("\\breg\\b\\s+(\\w+)"); // Matches reg declarations
        regex assignmentPattern("(\\w+)\\s*=\\s*"); // Matches assignments

        unordered_set<string> declaredRegisters; // All declared registers
        unordered_set<string> initializedRegisters; // All initialized registers

        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Capture all declared registers
            if (regex_search(line, match, regPattern)) {
                declaredRegisters.insert(match[1]);
            }

            // Capture all initialized registers
            if (regex_search(line, match, assignmentPattern)) {
                initializedRegisters.insert(match[1]);
            }
        }

        // Check for uninitialized registers
        for (const string& reg : declaredRegisters) {
            if (initializedRegisters.find(reg) == initializedRegisters.end()) {
                violations.push_back({ "Uninitialized register: " + reg, 0 });
            }
        }
    }

    void checkXPropagation() {
        regex xPattern("\\b(\\d+'[bB]?[xX]+)\\b|\\b[Xx]\\b");
        regex bitwisePattern("=\\s*(.+)\\s*;");

        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Check for X literals or standalone X in the line
            if (regex_search(line, match, xPattern)) {
                // Also check for bitwise operators
                if (regex_search(line, match, bitwisePattern)) {
                    string expression = match[1];
                    if (evaluateExpressionForX(expression)) {
                        violations.push_back({ "X propagation or reachability issue in expression: " + expression, i + 1 });
                    }
                }
                else {
                    // Directly report standalone X
                    violations.push_back({ "X propagation or reachability issue", i + 1 });
                }
            }
        }
    }
// Case Statement Checks
void checkCaseStatements() {
    regex casePattern(R"(case\s*\((\w+)\))");
    regex endCasePattern(R"(endcase)");
    regex defaultPattern(R"(default:)");
    regex conditionPattern(R"(\s*(\d+'[bB]?\w+|[a-zA-Z_]\w*)\s*:)");

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = lines[i];
        smatch match;

        // Detect the start of a case statement
        if (regex_search(line, match, casePattern)) {
            unordered_set<string> cases;
            bool hasDefault = false;
            size_t caseStartLine = i;

            // Traverse lines to find the end of the case block
            while (++i < lines.size()) {
                line = lines[i];
                // Check for `default` case
                if (regex_search(line, defaultPattern)) {
                    hasDefault = true;
                }

                // Check for conditions
                smatch conditionMatch;
                if (regex_search(line, conditionMatch, conditionPattern)) {
                    string condition = conditionMatch[1];
                    if (cases.find(condition) != cases.end()) {
                        violations.push_back({ "Duplicate condition in case statement: " + condition, i + 1 });
                    }
                    cases.insert(condition);
                }

                // Break on `endcase`
                if (regex_search(line, endCasePattern)) {
                    break;
                }
            }

            // Check for missing default case
            if (!hasDefault) {
                violations.push_back({ "Missing default case in case statement starting at line " + to_string(caseStartLine + 1), caseStartLine + 1 });
            }
        }
    }
}

// Arithmetic Overflow Checks
void checkArithmeticOverflow() {
    regex arithmeticPattern(R"(\s*(\w+)\s*=\s*(.+);)");
    regex operationPattern(R"((\w+)\s*([\+\-\*/])\s*(\w+))");

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = lines[i];
        smatch match;

        // Detect assignment statements
        if (regex_search(line, match, arithmeticPattern)) {
            string expression = match[2];

            // Check for arithmetic operations
            smatch opMatch;
            if (regex_search(expression, opMatch, operationPattern)) {
                string operand1 = opMatch[1];
                string operation = opMatch[2];
                string operand2 = opMatch[3];

                // Simplified logic: Assume 32-bit operands for this example
                int bitWidth = 32; // Assume 32-bit operands for simplicity
                bool overflow = false;

                if (operation == "+" || operation == "-") {
                    // Check if the result exceeds bit width
                    overflow = (operand1.length() > bitWidth || operand2.length() > bitWidth);
                } else if (operation == "*" || operation == "/") {
                    // Multiplication or division overflow checks
                    overflow = (operand1.length() * operand2.length() > bitWidth);
                }

                if (overflow) {
                    violations.push_back({ "Potential arithmetic overflow in operation: " + operand1 + " " + operation + " " + operand2, i + 1 });
                }
            }
        }
    }
}


    bool evaluateExpressionForX(const string& expression) {
        unordered_map<char, unordered_map<char, char>> andTruthTable = {
            {'0', {{'0', '0'}, {'1', '0'}, {'x', '0'}, {'z', '0'}}},
            {'1', {{'0', '0'}, {'1', '1'}, {'x', 'x'}, {'z', 'x'}}},
            {'x', {{'0', '0'}, {'1', 'x'}, {'x', 'x'}, {'z', 'x'}}},
            {'z', {{'0', '0'}, {'1', 'x'}, {'x', 'x'}, {'z', 'x'}}}
        };

        unordered_map<char, unordered_map<char, char>> orTruthTable = {
            {'0', {{'0', '0'}, {'1', '1'}, {'x', 'x'}, {'z', 'x'}}},
            {'1', {{'0', '1'}, {'1', '1'}, {'x', '1'}, {'z', '1'}}},
            {'x', {{'0', 'x'}, {'1', '1'}, {'x', 'x'}, {'z', 'x'}}},
            {'z', {{'0', 'x'}, {'1', '1'}, {'x', 'x'}, {'z', 'x'}}}
        };

        auto evaluate = [&](const string& expr) -> char {
            string simplified = expr;
            for (size_t i = 0; i < simplified.size(); ++i) {
                if (simplified[i] == '&' || simplified[i] == '|') {
                    char left = simplified[i - 1];
                    char op = simplified[i];
                    char right = simplified[i + 1];

                    char result = (op == '&') ? andTruthTable[left][right] : orTruthTable[left][right];
                    simplified.replace(i - 1, 3, 1, result);
                    i = 0; // Restart evaluation
                }
            }
            return simplified[0];
            };

        string preprocessed;
        for (char c : expression) {
            if (isdigit(c) || c == 'x' || c == 'X' || c == 'z' || c == 'Z' || c == '&' || c == '|') {
                preprocessed += tolower(c);
            }
        }

        char result = evaluate(preprocessed);
        return result == 'x';
    }

    // Combinational Loop Checks
    void checkCombinationalLoops() {
        // Data structure to represent the dependency graph
        unordered_map<string, vector<string>> dependencyGraph;

        // Regular expressions to capture assignments
        regex assignPattern(R"(assign\s+(\w+)\s*=\s*(.+);)");

        // Build the dependency graph
        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Check for assign statements
            if (regex_search(line, match, assignPattern)) {
                string lhs = match[1];        // Left-hand side (output)
                string rhs = match[2];        // Right-hand side (expression)

                // Extract all variables used in the right-hand side
                regex varPattern(R"(\b\w+\b)");
                auto varsBegin = sregex_iterator(rhs.begin(), rhs.end(), varPattern);
                auto varsEnd = sregex_iterator();

                for (auto it = varsBegin; it != varsEnd; ++it) {
                    string var = it->str();
                    if (var != lhs) { // Avoid self-references
                        dependencyGraph[lhs].push_back(var);
                    }
                }
            }
        }

        // Declare the recursive function using std::function
        std::function<bool(const string&, unordered_set<string>&, unordered_set<string>&)> hasCycle =
            [&](const string& node, unordered_set<string>& visited, unordered_set<string>& recursionStack) -> bool {
            if (recursionStack.find(node) != recursionStack.end()) {
                // Node is already in the recursion stack, indicating a cycle
                return true;
            }

            if (visited.find(node) != visited.end()) {
                // Node is already visited, no need to check further
                return false;
            }

            // Mark the node as visited and add to recursion stack
            visited.insert(node);
            recursionStack.insert(node);

            // Recursively check all neighbors
            for (const string& neighbor : dependencyGraph[node]) {
                if (hasCycle(neighbor, visited, recursionStack)) {
                    return true;
                }
            }

            // Remove the node from recursion stack
            recursionStack.erase(node);
            return false;
            };

        // Check for cycles in the graph
        unordered_set<string> visited;
        unordered_set<string> recursionStack;

        for (const auto& entry : dependencyGraph) {
            const string& node = entry.first;
            if (visited.find(node) == visited.end() && hasCycle(node, visited, recursionStack)) {
                violations.push_back({ "Combinational loop detected involving node: " + node, 0 });
            }
        }
    }


public:
    explicit StaticChecker(const vector<string>& lines) : lines(lines) {}

    void runChecks() {
        checkUninitializedRegisters();
        checkXPropagation();
        checkCombinationalLoops();
        checkCaseStatements();
        checkArithmeticOverflow();
    }

    void reportViolations() const {
        if (violations.empty()) {
            cout << "No violations found!" << endl;
        }
        else {
            cout << "Violations found:" << endl;
            for (const auto& violation : violations) {
                cout << "Line " << (violation.line ? to_string(violation.line) : "unknown") << ": " << violation.message << endl;
            }
        }
    }
};

// Main Program
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <verilog_file>" << endl;
        return EXIT_FAILURE;
    }

    string filename = argv[1];

    // Parse Verilog File
    VerilogParser parser(filename);

    // Perform Static Checks
    StaticChecker checker(parser.getLines());
    checker.runChecks();

    // Report Violations
    checker.reportViolations();

    return 0;
}
