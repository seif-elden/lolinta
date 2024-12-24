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
#include <stack> 


using namespace std;

// Utility function to remove comments from Verilog code (Helper Function For latch Inference)
string removeComments(const string& code) {
    string cleanedCode = code;
    // Remove single-line comments (//)
    cleanedCode = regex_replace(cleanedCode, regex(R"(//.*)"), "");
    // Remove block comments (/* ... */)
    cleanedCode = regex_replace(cleanedCode, regex(R"(/\*[\s\S]*?\*/)"), "");
    return cleanedCode;
}


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

    // UnreachableFSM Checks
    void checkUnreachableFSMStates() {
        unordered_set<string> availableStates;                      // States defined in the case block
        unordered_set<string> nextStates;                           // States transitioned to
        unordered_map<string, int> stateLineMap;                    // Map to track the line number of each state

        // Regular expression to parse FSM case states and transitions
        regex stateRegex(R"((\w+)\s*:\s*state\s*<=\s*(\w+);)");

        // Iterate through each line of the Verilog code
        for (size_t i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Parse states in the case block
            if (regex_search(line, match, stateRegex)) {
                string availableState = match[1];  // State in the case block
                string nextState = match[2];       // State transitioned to

                availableStates.insert(availableState);
                nextStates.insert(nextState);

                // Map the state to its line number
                if (stateLineMap.find(availableState) == stateLineMap.end()) {
                    stateLineMap[availableState] = i + 1;  // Line numbers are 1-based
                }


            }
        }

        // Identify unreachable states
        for (const auto& state : availableStates) {
            if (nextStates.find(state) == nextStates.end()) {
                // If a state is defined in availableStates but not in nextStates, it's unreachable
                int line = stateLineMap[state];  // Get the line number of the state
                violations.push_back({
                    "Unreachable FSM state: " + state,
                    line  // Line number of the unreachable state
                    });
            }
        }
    }


    // Check Latch Inference
    void checkLatchInference() {
        string verilogCode;
        for (const auto& line : lines) {
            verilogCode += line + "\n";
        }

        // Remove comments from the Verilog code to avoid false matches
        verilogCode = removeComments(verilogCode);

        // Regular expression to match always blocks
        regex alwaysBlockRegex(R"(always\s*@\*\s*begin([\s\S]*?)end\s*)");
        smatch alwaysBlockMatch;
        vector<string> alwaysBlocks;

        // Extract all always blocks
        auto codeBegin = verilogCode.cbegin();
        auto codeEnd = verilogCode.cend();
        while (regex_search(codeBegin, codeEnd, alwaysBlockMatch, alwaysBlockRegex)) {
            alwaysBlocks.push_back(alwaysBlockMatch[1].str());
            codeBegin = alwaysBlockMatch.suffix().first;
        }

        // Process each always block
        for (const auto& block : alwaysBlocks) {
            istringstream blockStream(block);
            string currentLine;
            stack<bool> ifHasElseStack;  // Tracks if each if has an else
            stack<int> blockDepthStack; // Tracks nested block depths
            bool latchDetected = false;
            bool caseMissingDefault = false;

            int currentDepth = 0;
            int lineNumber = 0;
            bool insideCase = false; // Tracks if we are inside a case statement

            while (getline(blockStream, currentLine)) {
                ++lineNumber;

                // Remove leading/trailing whitespace for cleaner processing
                currentLine = regex_replace(currentLine, regex(R"(^\s+|\s+$)"), "");

                // Match patterns for Verilog constructs
                regex ifRegex(R"(\bif\s*\()");
                regex elseRegex(R"(\belse\b)");
                regex beginRegex(R"(\bbegin\b)");
                regex endRegex(R"(\bend\b)");
                regex caseRegex(R"(\bcase\b)");
                regex endcaseRegex(R"(\bendcase\b)");
                regex defaultRegex(R"(\bdefault\b)");

                // Handle "begin" statements (nested blocks)
                if (regex_search(currentLine, beginRegex)) {
                    ++currentDepth;
                }
                // Handle "end" statements
                else if (regex_search(currentLine, endRegex)) {
                    if (!blockDepthStack.empty() && blockDepthStack.top() == currentDepth) {
                        blockDepthStack.pop();
                        if (!ifHasElseStack.empty()) {
                            ifHasElseStack.pop();
                        }
                    }
                    --currentDepth;
                }
                // Handle "if" statements
                else if (regex_search(currentLine, ifRegex)) {
                    ifHasElseStack.push(false); // Push a new if without an else initially
                    blockDepthStack.push(currentDepth); // Track its block depth
                }
                // Handle "else" statements
                else if (regex_search(currentLine, elseRegex)) {
                    if (!ifHasElseStack.empty() && blockDepthStack.top() == currentDepth) {
                        ifHasElseStack.top() = true; // Mark the most recent if as having an else
                    }
                }
                // Handle "case" statements
                else if (regex_search(currentLine, caseRegex)) {
                    insideCase = true;
                    caseMissingDefault = true; // Assume missing default until proven otherwise
                }
                // Handle "default" within a case statement
                else if (insideCase && regex_search(currentLine, defaultRegex)) {
                    caseMissingDefault = false; // Default branch found
                }
                // Handle "endcase"
                else if (regex_search(currentLine, endcaseRegex)) {
                    if (insideCase && caseMissingDefault) {
                        violations.push_back({ "Missing default branch in case statement.", lineNumber });
                    }
                    insideCase = false; // Exit the case context
                }
            }

            // After processing, any if without an else means a latch
            while (!ifHasElseStack.empty()) {
                if (!ifHasElseStack.top() && !latchDetected) {
                    violations.push_back({ "Potential inferred latch found in always block.", lineNumber });
                    latchDetected = true;
                }
                ifHasElseStack.pop();
            }
        }
    }

    // Initialization Checks
    void checkUninitializedRegisters() {
        // Updated regex to capture registers with optional ranges (e.g., reg [3:0] reg1;)
        regex regPattern("\\breg\\b(\\s*\\[[^\\]]+\\])?\\s+(\\w+);");
        // Updated regex to capture assignments (e.g., reg1 = <value>;)
        regex assignmentPattern("(\\w+)\\s*=\\s*");

        unordered_set<string> initializedRegisters; // All initialized registers
        unordered_map<string, int> declaredRegisters; // Map of declared registers and their line numbers

        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Capture all declared registers and their line numbers
            if (regex_search(line, match, regPattern)) {
                string regName = match[2]; // The name of the register
                if (declaredRegisters.find(regName) == declaredRegisters.end()) {
                    declaredRegisters[regName] = i + 1; // Store the line number (1-based index)
                }
            }

            // Capture all initialized registers
            if (regex_search(line, match, assignmentPattern)) {
                string regName = match[1]; // The name of the register being assigned
                initializedRegisters.insert(regName);
            }
        }

        // Check for uninitialized registers
        for (const auto& regEntry : declaredRegisters) {
            const string& regName = regEntry.first;
            int lineNumber = regEntry.second;
            if (initializedRegisters.find(regName) == initializedRegisters.end()) {
                violations.push_back({
                    "Uninitialized register: " + regName,
                    lineNumber
                    });
            }
        }
    }

    // X Propagation Checks
    void checkXPropagation() {
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

        regex assignRegex(R"(assign\s+(\w+)\s*=\s*(.+);)");
        smatch match;

        for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum) {
            string line = lines[lineNum];

            // Remove comments from the line
            line = removeComments(line);

            if (regex_search(line, match, assignRegex)) {
                string target = match[1];
                string expression = match[2];

                // Check for direct X propagation (e.g., assign c = 4'bxxxx;)
                if (regex_search(expression, regex(R"(\b1'bx|4'bx{4}\b)"))) {
                    violations.push_back({ "Direct X propagation to " + target, static_cast<int>(lineNum + 1) });
                    continue;
                }

                // Evaluate the expression for bitwise operations
                stack<char> evaluationStack;
                for (char ch : expression) {
                    if (isdigit(ch) || ch == 'x' || ch == 'z') {
                        evaluationStack.push(ch);
                    }
                    else if (ch == '&' || ch == '|') {
                        if (evaluationStack.size() < 2) {
                      
                            break;
                        }

                        char b = evaluationStack.top(); evaluationStack.pop();
                        char a = evaluationStack.top(); evaluationStack.pop();
                        char result = 'x';

                        if (ch == '&') {
                            result = andTruthTable[a][b];
                        }
                        else if (ch == '|') {
                            result = orTruthTable[a][b];
                        }

                        evaluationStack.push(result);
                    }
                }

                // Check the final result for X propagation
                if (!evaluationStack.empty() && evaluationStack.top() == 'x') {
                    violations.push_back({ "X propagation detected in expression assigned to " + target, static_cast<int>(lineNum + 1) });
                }
            }
        }
    }

    // Combinational Loop Checks
    void checkCombinationalLoops() {
        // Data structure to represent the dependency graph
        unordered_map<string, vector<string>> dependencyGraph;

        // Regular expressions to capture assignments
        regex assignPattern(R"(assign\s+(\w+)\s*=\s*(.+);)");

        // Build the dependency graph
        unordered_map<string, int> lineMap; // Map to store variable declaration line numbers
        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Check for assign statements
            if (regex_search(line, match, assignPattern)) {
                string lhs = match[1];        // Left-hand side (output)
                string rhs = match[2];        // Right-hand side (expression)

                // Record the line number for the left-hand side variable
                lineMap[lhs] = i + 1;

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

        // Declare the recursive function using function
        function<bool(const string&, unordered_set<string>&, unordered_set<string>&, string&)> hasCycle =
            [&](const string& node, unordered_set<string>& visited, unordered_set<string>& recursionStack, string& firstNode) -> bool {
            if (recursionStack.find(node) != recursionStack.end()) {
                // Node is already in the recursion stack, indicating a cycle
                firstNode = node;
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
                if (hasCycle(neighbor, visited, recursionStack, firstNode)) {
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
            if (visited.find(node) == visited.end()) {
                string firstNode;
                if (hasCycle(node, visited, recursionStack, firstNode)) {
                    int lineNum = lineMap.count(firstNode) ? lineMap[firstNode] : 0;
                    violations.push_back({ "Combinational loop detected involving node: " + firstNode, lineNum });
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

        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Detect the start of a case statement
            if (regex_search(line, match, casePattern)) {
                unordered_set<string> cases;
                bool hasDefault = false;
                int caseStartLine = i;

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

    // Function to detect unreachable branches
    void checkDeadCode() {
        regex regRegex(R"(\s*reg\s*\[\s*(\d+)\s*:\s*0\s*\]\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*;)");
        regex caseRegex(R"(\s*case\s*\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\))");
        regex caseBranchRegex(R"(\s*(\d+'b[01]+)\s*:.*)");
        regex ifElseRegex(R"(\s*(if|else if|else)\s*\(?\s*.*?\)?\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(\d+'b[01]+)\s*;)");
        regex ifConditionRegex(R"(\s*(if|else if)\s*\(\s*(.*?)\s*\))"); // To match if and else if conditions

        string currentSelector;
        unordered_set<string> reachableValues; // Track reachable values
        vector<pair<string, size_t>> caseBranches; // Values from case statement with line numbers
        bool inCaseBlock = false;
        unordered_set<int> unreachableIfElseLines; // To track unreachable if-else lines

        for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
            string line = lines[lineNumber];
            smatch match;

            // Detect register declaration
            if (regex_search(line, match, regRegex)) {
                currentSelector = match[2];
            }

            // Track assignments to the selector variable
            if (regex_search(line, match, ifElseRegex)) {
                if (match[2] == currentSelector) {
                    reachableValues.insert(match[3]); // Add assigned value
                }
            }

            // Start analyzing a case block
            if (regex_search(line, match, caseRegex)) {
                if (match[1] == currentSelector) {
                    inCaseBlock = true;
                    caseBranches.clear();
                }
            }

            // Collect values in case branches with line numbers
            if (inCaseBlock && regex_search(line, match, caseBranchRegex)) {
                caseBranches.emplace_back(match[1], lineNumber + 1);
            }

            // End of case block
            if (inCaseBlock && line.find("endcase") != string::npos) {
                inCaseBlock = false;

                // Compare reachable values with case branches
                unordered_set<size_t> unreachableBranchLines;
                for (const auto& branch : caseBranches) {
                    if (reachableValues.find(branch.first) == reachableValues.end()) {
                        unreachableBranchLines.insert(branch.second);
                    }
                }

                // Report unreachable branches with line numbers
                for (const auto& branchLine : unreachableBranchLines) {
                    violations.push_back({ "Unreachable 'case' branch at line: " + to_string(branchLine), static_cast<int>(branchLine) });
                }
            }

            // Check for unreachable if-else conditions
            if (regex_search(line, match, ifConditionRegex)) {
                string condition = match[2];

                // For simplicity, assume conditions that are always false (simplified analysis)
                if (condition == "1'b0" || condition == "0") { // Example: Always false condition
                    unreachableIfElseLines.insert(lineNumber + 1); // Mark this line as unreachable
                }
            }
        }

        // Report unreachable if-else branches
        for (const auto& line : unreachableIfElseLines) {
            violations.push_back({ "Unreachable if-else statement at line: " + to_string(line), line });
        }
    }

    // Arithmetic Overflow Checks
    void checkArithmeticOverflow() {
        // Match for variable declarations with bit-widths, including multiple variables in one line.
        regex declarationPattern(R"(reg\s*\[(\d+):(\d+)\]\s*(\w+)(?:\s*,\s*(\w+))*;)");
        // Matches: reg [3:0] a, b; (including multiple variables)

        regex assignmentPattern(R"(\s*(\w+)\s*=\s*(.+);)");          // Matches assignment statements
        regex operationPattern(R"((\w+|\d+|\d+'\w+)\s*([\+\-\*/])\s*(\d+'\w+|\d+|\w+))");
        // Matches variables (e.g., a), constants (e.g., 10, 4'b1101), and operations

        regex zeroPattern(R"(\d+'b0+$)");

        unordered_map<string, int> variableBitWidths; // Map to store bit widths for variables

        // Step 1: Extract bit widths from variable declarations
        for (const string& line : lines) {
            smatch match;
            if (regex_search(line, match, declarationPattern)) {
                int msb = stoi(match[1]); // Extract most significant bit
                int lsb = stoi(match[2]); // Extract least significant bit
                string variableName = match[3];
                int bitWidth = msb - lsb + 1; // Calculate bit width

                // Store the bit-width for the first variable
                variableBitWidths[variableName] = bitWidth;

                // Handle additional variables in the declaration
                for (size_t i = 4; i < match.size(); ++i) {
                    if (match[i].matched) {
                        variableBitWidths[match[i]] = bitWidth;
                    }
                }
            }
        }

        // Helper function to calculate bit-width of constants
        auto getBitWidthForConstant = [](const string& constant) {
            if (constant.find("'") != string::npos) {
                // If it's a Verilog literal (e.g., 4'b1101 or 8'hFF)
                size_t delimiterPos = constant.find("'");

                // Extract the bit width before the `'`
                int width = stoi(constant.substr(0, delimiterPos));


                return width;
            }
            else {
                // Otherwise, it's a decimal constant, calculate bit width based on its value
                int value = stoi(constant);
                int width = 0;
                while (value > 0) {
                    value >>= 1;
                    width++;
                }
                return width > 0 ? width : 1; // At least 1 bit for zero
            }
            };



        // Step 2: Detect arithmetic operations and check for overflow
        for (size_t i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Detect assignment statements
            if (regex_search(line, match, assignmentPattern)) {
                string destination = match[1];  // Left-hand side of assignment
                string expression = match[2];  // Right-hand side (arithmetic operation)

                smatch opMatch;
                if (regex_search(expression, opMatch, operationPattern)) {
                    string operand1 = opMatch[1];
                    string operation = opMatch[2];
                    string operand2 = opMatch[3];



                    // Determine bit widths of the operands
                    int bitWidth1 = 0;
                    int bitWidth2 = 0;

                    // Check if operand1 is a constant or variable
                    if (operand1.find("'") != string::npos || isdigit(operand1[0])) {
                        bitWidth1 = getBitWidthForConstant(operand1); // Operand1 is a constant
                    }
                    else {
                        bitWidth1 = variableBitWidths.count(operand1) ? variableBitWidths[operand1] : 8; // Operand1 is a variable
                    }

                    // Check if operand2 is a constant or variable
                    if (operand2.find("'") != string::npos || isdigit(operand2[0])) {
                        bitWidth2 = getBitWidthForConstant(operand2); // Operand2 is a constant
                    }
                    else {
                        bitWidth2 = variableBitWidths.count(operand2) ? variableBitWidths[operand2] : 8; // Operand2 is a variable
                    }

                    int resultBitWidth = variableBitWidths.count(destination) ? variableBitWidths[destination] : 8;




                    bool overflow = false;

                    // Check for overflow conditions
                    if (operation == "+") {
                        overflow = (max(bitWidth1, bitWidth2) + 1 > resultBitWidth); // +1 for carry
                    }
                    else if (operation == "-") {
                        overflow = (bitWidth1 >  bitWidth2) ;
                    }
                    else if (operation == "*") {
                        overflow = (bitWidth1 + bitWidth2 > resultBitWidth);
                    }
                    else if (operation == "/") {
                        overflow = regex_match(operand2, zeroPattern) ? 1 : 0; // Division by zero check
                    }

                    // Report potential overflow
                    if (overflow) {
                        violations.push_back({
                            "Potential arithmetic overflow in operation: " + operand1 + " " + operation + " " + operand2,
                            static_cast<int>(i + 1)
                            });
                    }
                }
            }
        }
    }

    void checkMultiDrivenBus() {
        regex assignPattern(R"(assign\s+(\w+)\s*=\s*(.*?);)"); // Matches 'assign bus = ...;'
        unordered_map<string, vector<string>> busAssignments;   // Tracks conditions per bus

        for (size_t i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            smatch match;

            // Search for 'assign' statements
            if (regex_search(line, match, assignPattern)) {
                string busName = match[1];      // Extract the bus name
                string condition = match[2];    // Extract the assigned condition or value

                busAssignments[busName].push_back(condition);

                // Check for conflicting drivers
                if (busAssignments[busName].size() > 1) {
                    violations.push_back({ "Bus value conflict detected: " + busName, static_cast<int>(i + 1) });
                }
            }
        }
    }



public:
    explicit StaticChecker(const vector<string>& lines) : lines(lines) {}

    void runChecks() {
        checkUnreachableFSMStates();
        checkUninitializedRegisters();
        checkLatchInference();
        checkXPropagation();
        checkCombinationalLoops();
        checkCaseStatements();
        checkDeadCode();
        checkArithmeticOverflow();
        checkMultiDrivenBus();

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