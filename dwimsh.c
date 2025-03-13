#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <pwd.h>
#include <termios.h>
#include <libgen.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <math.h>

#define MAX_CMD_LENGTH 1024
#define MAX_CMDS 2048
#define MAX_PATH_LENGTH 1024
#define MAX_RECOMMENDATIONS 100
#define HISTORY_FILE ".dwimsh_history"
#define LEVENSHTEIN_THRESHOLD 0.4  // Levenshtein threshold for recommendations

// ANSI color codes
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"

typedef struct {
    char *commands[MAX_CMDS];
    int count;
} CommandTable;

CommandTable cmdTable;
char *homeDir;
char historyFilePath[MAX_PATH_LENGTH];
int interactive = 1;

// Function declarations
void LoadCommands();
int IsCommandInTable(const char *cmd);
void FreeCommandsMemory();
void TokenizeUserInput(char *command, char **tokens, int *tokenCount);
void PrintTokens(char **tokens, int tokenCount);
int HammingDistance(const char *str1, const char *str2);
int LevenshteinDistance(const char *s1, const char *s2);
int AreAnagrams(const char *str1, const char *str2);
void FindSimilarCommands(const char *cmd, char **recommendations, int *recommendationCount);
void JoinUserRecommendation(char *recommendation, char **tokens, int tokenCount, char *newCommand);
void ListCommandsTable();
void DeleteDuplicatedRecommendations(char **recommendations, int *recommendationCount);
int ExecuteCommand(char **tokens, int tokenCount);
int IsBuiltInCommand(const char *cmd);
int IsYesResponse(const char *response);
int IsNoResponse(const char *response);
void HandleSignal(int sig);
void InitHistory();
void SaveHistory();
void PrintWelcomeMessage();
void PrintHelpMessage();
void Cleanup();
char *GetPrompt();
void PrintColoredText(const char *text, const char *color);

// Signal handler for clean exit
void HandleSignal(int sig) {
    if (sig == SIGINT) {
        printf("\n");
        if (interactive) {
            char *prompt = GetPrompt();
            printf("%s", prompt);
            free(prompt);
            fflush(stdout);
        }
    } else if (sig == SIGTERM) {
        Cleanup();
        exit(0);
    }
}

// Initialize command history
void InitHistory() {
    struct passwd *pw = getpwuid(getuid());
    homeDir = pw->pw_dir;
    
    snprintf(historyFilePath, MAX_PATH_LENGTH, "%s/%s", homeDir, HISTORY_FILE);
    
    using_history();
    read_history(historyFilePath);
    stifle_history(1000);  // Limit history to 1000 entries
}

// Save command history
void SaveHistory() {
    write_history(historyFilePath);
}

// Print welcome message with ASCII art
void PrintWelcomeMessage() {
    printf("\n");
    printf(COLOR_GREEN "██████╗ ██╗    ██╗██╗███╗   ███╗███████╗██╗  ██╗\n");
                printf("██╔══██╗██║    ██║██║████╗ ████║██╔════╝██║  ██║\n");
                printf("██║  ██║██║ █╗ ██║██║██╔████╔██║███████╗███████║\n");
                printf("██║  ██║██║███╗██║██║██║╚██╔╝██║╚════██║██╔══██║\n");
                printf("██████╔╝╚███╔███╔╝██║██║ ╚═╝ ██║███████║██║  ██║\n");
                printf("╚═════╝  ╚══╝╚══╝ ╚═╝╚═╝     ╚═╝╚══════╝╚═╝  ╚═╝\n" COLOR_RESET);
    printf(COLOR_GREEN "Do What I Mean Shell - Linux Edition v1.0\n" COLOR_RESET);
    printf(COLOR_GREEN "WRITTEN BY VÍCTOR ROMERO - 12211079\n\n" COLOR_RESET);
    printf(COLOR_YELLOW "Type 'help' for available commands or 'exit' to quit\n" COLOR_RESET);
    printf("\n");
}

// Print help message
void PrintHelpMessage() {
    printf("\n%sDWIMSH - Do What I Mean Shell%s\n\n", COLOR_BOLD, COLOR_RESET);
    printf("Built-in commands:\n");
    printf("  %sexit%s          - Exit the shell\n", COLOR_BOLD, COLOR_RESET);
    printf("  %shelp%s          - Display this help message\n", COLOR_BOLD, COLOR_RESET);
    printf("  %sclear%s         - Clear the screen\n", COLOR_BOLD, COLOR_RESET);
    printf("  %slist%s          - List all available commands\n", COLOR_BOLD, COLOR_RESET);
    printf("  %shistory%s       - Show command history\n", COLOR_BOLD, COLOR_RESET);
    printf("\n");
    printf("Features:\n");
    printf("  - Command correction using Hamming distance\n");
    printf("  - Command correction using Levenshtein distance\n");
    printf("  - Command correction using anagram detection\n");
    printf("  - Command history with up/down arrow keys\n");
    printf("  - Tab completion for commands\n");
    printf("\n");
}

// Clean up resources
void Cleanup() {
    SaveHistory();
    FreeCommandsMemory();
    clear_history();
}

// Get current directory for prompt
char *GetPrompt() {
    char cwd[MAX_PATH_LENGTH];
    char *prompt = malloc(MAX_PATH_LENGTH);
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "unknown");
    }
    
    // If we're in home directory, use ~ instead
    if (strncmp(cwd, homeDir, strlen(homeDir)) == 0) {
        char *home_relative = cwd + strlen(homeDir);
        if (*home_relative == '\0') {
            sprintf(prompt, COLOR_GREEN "dwimsh" COLOR_YELLOW ":" COLOR_BLUE "~" COLOR_RESET "$ ");
        } else {
            sprintf(prompt, COLOR_GREEN "dwimsh" COLOR_YELLOW ":" COLOR_BLUE "~%s" COLOR_RESET "$ ", home_relative);
        }
    } else {
        sprintf(prompt, COLOR_GREEN "dwimsh" COLOR_YELLOW ":" COLOR_BLUE "%s" COLOR_RESET "$ ", basename(cwd));
    }
    
    return prompt;
}

// Print colored text
void PrintColoredText(const char *text, const char *color) {
    printf("%s%s%s", color, text, COLOR_RESET);
}

// Load commands from directories in PATH
void LoadCommands() {
    char *path = getenv("PATH");
    char *pathCopy = strdup(path);
    char *dir = strtok(pathCopy, ":");
    DIR *dirp;
    struct dirent *entry;
    
    cmdTable.count = 0;
    
    while (dir != NULL && cmdTable.count < MAX_CMDS) {
        dirp = opendir(dir);
        if (dirp != NULL) {
            while ((entry = readdir(dirp)) != NULL && cmdTable.count < MAX_CMDS) {
                if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
                    // Check if file is executable
                    char fullPath[MAX_PATH_LENGTH];
                    snprintf(fullPath, MAX_PATH_LENGTH, "%s/%s", dir, entry->d_name);
                    if (access(fullPath, X_OK) == 0) {
                        cmdTable.commands[cmdTable.count] = strdup(entry->d_name);
                        cmdTable.count++;
                    }
                }
            }
            closedir(dirp);
        }
        dir = strtok(NULL, ":");
    }
    
    free(pathCopy);
    
    // Sort commands alphabetically for easier searching
    for (int i = 0; i < cmdTable.count; i++) {
        for (int j = i + 1; j < cmdTable.count; j++) {
            if (strcmp(cmdTable.commands[i], cmdTable.commands[j]) > 0) {
                char *temp = cmdTable.commands[i];
                cmdTable.commands[i] = cmdTable.commands[j];
                cmdTable.commands[j] = temp;
            }
        }
    }
    
    // Add built-in commands
    if (cmdTable.count < MAX_CMDS) {
        cmdTable.commands[cmdTable.count++] = strdup("exit");
    }
    if (cmdTable.count < MAX_CMDS) {
        cmdTable.commands[cmdTable.count++] = strdup("help");
    }
    if (cmdTable.count < MAX_CMDS) {
        cmdTable.commands[cmdTable.count++] = strdup("clear");
    }
    if (cmdTable.count < MAX_CMDS) {
        cmdTable.commands[cmdTable.count++] = strdup("list");
    }
    if (cmdTable.count < MAX_CMDS) {
        cmdTable.commands[cmdTable.count++] = strdup("history");
    }
}

// Check if command exists in the command table
int IsCommandInTable(const char *cmd) {
    if (cmd == NULL || *cmd == '\0')
        return 0;
        
    for (int i = 0; i < cmdTable.count; i++) {
        if (strcmp(cmdTable.commands[i], cmd) == 0) {
            return 1;
        }
    }
    return 0;
}

// Check if command is a built-in command
int IsBuiltInCommand(const char *cmd) {
    if (cmd == NULL || *cmd == '\0')
        return 0;
        
    return (strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "help") == 0 ||
            strcmp(cmd, "clear") == 0 ||
            strcmp(cmd, "list") == 0 ||
            strcmp(cmd, "history") == 0);
}

// Free memory from command table
void FreeCommandsMemory() {
    for (int i = 0; i < cmdTable.count; i++) {
        free(cmdTable.commands[i]);
    }
}

// Split command into tokens
void TokenizeUserInput(char *command, char **tokens, int *tokenCount) {
    *tokenCount = 0;
    char *token = strtok(command, " \t");
    
    while (token != NULL && *tokenCount < MAX_CMD_LENGTH - 1) {
        tokens[*tokenCount] = token;
        (*tokenCount)++;
        token = strtok(NULL, " \t");
    }
    
    tokens[*tokenCount] = NULL;
}

// Print command tokens
void PrintTokens(char **tokens, int tokenCount) {
    for (int i = 0; i < tokenCount; i++) {
        printf("Token %d: %s\n", i, tokens[i]);
    }
}

// Calculate Hamming Distance between two strings
int HammingDistance(const char *str1, const char *str2) {
    if (strlen(str1) != strlen(str2))
        return -1;
        
    int distance = 0;
    for (int i = 0; i < strlen(str1); i++) {
        if (str1[i] != str2[i]) {
            distance++;
        }
    }
    return distance;
}

// Calculate Levenshtein Distance between two strings
int LevenshteinDistance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    int matrix[len1 + 1][len2 + 1];
    
    for (int i = 0; i <= len1; i++)
        matrix[i][0] = i;
    
    for (int j = 0; j <= len2; j++)
        matrix[0][j] = j;
    
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            
            int delete_cost = matrix[i-1][j] + 1;
            int insert_cost = matrix[i][j-1] + 1;
            int subst_cost = matrix[i-1][j-1] + cost;
            
            int min = delete_cost;
            if (insert_cost < min) min = insert_cost;
            if (subst_cost < min) min = subst_cost;
            
            matrix[i][j] = min;
        }
    }
    
    return matrix[len1][len2];
}

// Check if two strings are anagrams
int AreAnagrams(const char *str1, const char *str2) {
    if (strlen(str1) != strlen(str2))
        return 0;
        
    int counts[256] = {0};
    
    for (int i = 0; i < strlen(str1); i++) {
        counts[(unsigned char)str1[i]]++;
        counts[(unsigned char)str2[i]]--;
    }
    
    for (int i = 0; i < 256; i++) {
        if (counts[i] != 0)
            return 0;
    }
    return 1;
}

// Find similar commands using multiple algorithms
void FindSimilarCommands(const char *cmd, char **recommendations, int *recommendationCount) {
    *recommendationCount = 0;
    
    // Skip empty commands
    if (cmd == NULL || *cmd == '\0')
        return;
    
    for (int i = 0; i < cmdTable.count && *recommendationCount < MAX_RECOMMENDATIONS; i++) {
        // Skip very short commands (less than 2 chars)
        if (strlen(cmdTable.commands[i]) < 2)
            continue;
            
        // Check Hamming distance for same-length strings
        if (strlen(cmd) == strlen(cmdTable.commands[i])) {
            int distance = HammingDistance(cmd, cmdTable.commands[i]);
            if (distance >= 0 && distance <= strlen(cmd) * 0.5) {
                recommendations[*recommendationCount] = strdup(cmdTable.commands[i]);
                (*recommendationCount)++;
                continue;
            }
        }
        
        // Check Levenshtein distance for strings of different lengths
        int distance = LevenshteinDistance(cmd, cmdTable.commands[i]);
        float normalized_distance = (float)distance / (float)fmax(strlen(cmd), strlen(cmdTable.commands[i]));
        if (normalized_distance <= LEVENSHTEIN_THRESHOLD) {
            recommendations[*recommendationCount] = strdup(cmdTable.commands[i]);
            (*recommendationCount)++;
            continue;
        }
        
        // Check for anagrams
        if (AreAnagrams(cmd, cmdTable.commands[i])) {
            recommendations[*recommendationCount] = strdup(cmdTable.commands[i]);
            (*recommendationCount)++;
            continue;
        }
        
        // Check for substring match (cmd is contained in command)
        if (strstr(cmdTable.commands[i], cmd) != NULL) {
            recommendations[*recommendationCount] = strdup(cmdTable.commands[i]);
            (*recommendationCount)++;
            continue;
        }
    }
}

// Join recommendation tokens into a string
void JoinUserRecommendation(char *recommendation, char **tokens, int tokenCount, char *newCommand) {
    strcpy(newCommand, recommendation);
    
    for (int i = 1; i < tokenCount; i++) {
        strcat(newCommand, " ");
        strcat(newCommand, tokens[i]);
    }
}

// Print the command table
void ListCommandsTable() {
    printf("Available commands (%d total):\n", cmdTable.count);
    
    int columns = 4;
    int rows = (cmdTable.count + columns - 1) / columns;
    int maxWidth = 0;
    
    // Find max command length for formatting
    for (int i = 0; i < cmdTable.count; i++) {
        int len = strlen(cmdTable.commands[i]);
        if (len > maxWidth) {
            maxWidth = len;
        }
    }
    
    maxWidth += 2;  // Add padding
    
    // Print in columns
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < columns; col++) {
            int index = col * rows + row;
            if (index < cmdTable.count) {
                printf("%-*s", maxWidth, cmdTable.commands[index]);
            }
        }
        printf("\n");
    }
}

// Remove duplicate recommendations
void DeleteDuplicatedRecommendations(char **recommendations, int *recommendationCount) {
    for (int i = 0; i < *recommendationCount; i++) {
        for (int j = i + 1; j < *recommendationCount; j++) {
            if (strcmp(recommendations[i], recommendations[j]) == 0) {
                free(recommendations[j]);
                
                for (int k = j; k < *recommendationCount - 1; k++) {
                    recommendations[k] = recommendations[k + 1];
                }
                
                (*recommendationCount)--;
                j--;
            }
        }
    }
}

// Check if response is some form of "yes"
int IsYesResponse(const char *response) {
    if (response == NULL)
        return 0;
        
    char lowerResponse[MAX_CMD_LENGTH];
    strcpy(lowerResponse, response);
    
    // Convert to lowercase
    for (int i = 0; i < strlen(lowerResponse); i++) {
        lowerResponse[i] = tolower(lowerResponse[i]);
    }
    
    // Check for various forms of "yes"
    return (strcmp(lowerResponse, "y") == 0 ||
            strcmp(lowerResponse, "yes") == 0 ||
            strcmp(lowerResponse, "yeah") == 0 ||
            strcmp(lowerResponse, "yep") == 0 ||
            strcmp(lowerResponse, "sure") == 0 ||
            strcmp(lowerResponse, "ok") == 0 ||
            strcmp(lowerResponse, "okay") == 0);
}

// Check if response is some form of "no"
int IsNoResponse(const char *response) {
    if (response == NULL)
        return 0;
        
    char lowerResponse[MAX_CMD_LENGTH];
    strcpy(lowerResponse, response);
    
    // Convert to lowercase
    for (int i = 0; i < strlen(lowerResponse); i++) {
        lowerResponse[i] = tolower(lowerResponse[i]);
    }
    
    // Check for various forms of "no"
    return (strcmp(lowerResponse, "n") == 0 ||
            strcmp(lowerResponse, "no") == 0 ||
            strcmp(lowerResponse, "nope") == 0 ||
            strcmp(lowerResponse, "nah") == 0);
}

// Tab completion function for readline
char *CommandGenerator(const char *text, int state) {
    static int list_index, len;
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    // Return the next name which partially matches
    while (list_index < cmdTable.count) {
        char *name = cmdTable.commands[list_index];
        list_index++;
        
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    
    return NULL;
}

// Initialize readline completion
void InitializeCompletion() {
    rl_attempted_completion_function = NULL;
    rl_completion_entry_function = CommandGenerator;
}

// Execute command with fork/exec
int ExecuteCommand(char **tokens, int tokenCount) {
    if (tokenCount == 0)
        return 0;
    
    // Handle built-in commands first, regardless of table
    if (strcmp(tokens[0], "exit") == 0) {
        return 1;  // Exit code
    } else if (strcmp(tokens[0], "help") == 0) {
        PrintHelpMessage();
        return 0;
    } else if (strcmp(tokens[0], "clear") == 0) {
        printf("\033[H\033[J");  // ANSI escape sequence to clear screen
        return 0;
    } else if (strcmp(tokens[0], "list") == 0) {
        ListCommandsTable();
        return 0;
    } else if (strcmp(tokens[0], "history") == 0) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; i++) {
                printf("%5d  %s\n", i + history_base, hist_list[i]->line);
            }
        }
        return 0;
    }
    
    // Only proceed if the command is in the table
    if (!IsCommandInTable(tokens[0])) {
        return -1;  // Command not found
    }
    
    // Execute external command
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork error");
        return 0;
    } else if (pid == 0) {
        // Child process
        execvp(tokens[0], tokens);
        perror("Command execution error");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return 0;
    }
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);
    
    // Initialize history
    InitHistory();
    
    // Load commands
    LoadCommands();
    
    // Set up tab completion
    InitializeCompletion();
    
    // Print welcome message
    PrintWelcomeMessage();
    
    char *input;
    int should_exit = 0;
    
    while (!should_exit) {
        // Get command using readline
        char *prompt = GetPrompt();
        input = readline(prompt);
        free(prompt);
        
        // Check for EOF (Ctrl+D)
        if (input == NULL) {
            printf("\n");
            break;
        }
        
        // Skip empty lines
        if (input[0] == '\0') {
            free(input);
            continue;
        }
        
        // Add to history if non-empty
        add_history(input);
        
        // Parse the command
        char *tokens[MAX_CMD_LENGTH];
        int tokenCount = 0;
        
        // Create a copy since strtok modifies the string
        char *inputCopy = strdup(input);
        TokenizeUserInput(inputCopy, tokens, &tokenCount);
        
        if (tokenCount > 0) {
            int result = ExecuteCommand(tokens, tokenCount);
            
            if (result == 1) {
                // Exit command
                should_exit = 1;
            } else if (result == -1) {
                // Command not found, look for recommendations
                printf(COLOR_RED "Command not found: %s\n" COLOR_RESET, tokens[0]);
                
                char *recommendations[MAX_RECOMMENDATIONS];
                int recommendationCount = 0;
                
                // Find similar commands
                FindSimilarCommands(tokens[0], recommendations, &recommendationCount);
                
                if (recommendationCount == 0) {
                    printf("No similar commands found. Please try again.\n");
                } else {
                    // Remove duplicates
                    DeleteDuplicatedRecommendations(recommendations, &recommendationCount);
                    
                    printf(COLOR_YELLOW "Found %d possible command%s:\n" COLOR_RESET, 
                           recommendationCount, 
                           recommendationCount == 1 ? "" : "s");
                    
                    char userInput[MAX_CMD_LENGTH];
                    for (int i = 0; i < recommendationCount; i++) {
                        printf(COLOR_CYAN "Did you mean: \"" COLOR_BOLD "%s", recommendations[i]);
                        
                        for (int j = 1; j < tokenCount; j++) {
                            printf(" %s", tokens[j]);
                        }
                        
                        printf(COLOR_RESET COLOR_CYAN "\"? [y/n] " COLOR_RESET);
                        
                        // Get user input
                        if (fgets(userInput, sizeof(userInput), stdin) == NULL) {
                            // Handle EOF
                            printf("\n");
                            for (int k = 0; k < recommendationCount; k++) {
                                free(recommendations[k]);
                            }
                            free(inputCopy);
                            free(input);
                            Cleanup();
                            return 0;
                        }
                        
                        // Remove newline
                        userInput[strcspn(userInput, "\n")] = 0;
                        
                        if (IsYesResponse(userInput)) {
                            // User accepted recommendation
                            char newCommand[MAX_CMD_LENGTH];
                            char *newTokens[MAX_CMD_LENGTH];
                            int newTokenCount = 0;
                            
                            // Create the command string
                            JoinUserRecommendation(recommendations[i], tokens, tokenCount, newCommand);
                            
                            printf(COLOR_GREEN "Executing: %s\n" COLOR_RESET, newCommand);
                            
                            // Parse the new command
                            char *newCommandCopy = strdup(newCommand);
                            TokenizeUserInput(newCommandCopy, newTokens, &newTokenCount);
                            
                            // Execute the command
                            int exec_result = ExecuteCommand(newTokens, newTokenCount);
                            if (exec_result == 1) {
                                should_exit = 1;
                            }
                            
                            free(newCommandCopy);
                            break;
                        } else if (IsNoResponse(userInput)) {
                            // Try next recommendation
                            continue;
                        } else {
                            printf(COLOR_RED "Please enter 'y' or 'n'.\n" COLOR_RESET);
                            i--;  // Repeat the current recommendation
                        }
                    }
                    
                    // Free recommendations
                    for (int i = 0; i < recommendationCount; i++) {
                        free(recommendations[i]);
                    }
                }
            }
        }
        
        free(inputCopy);
        free(input);
    }
    
    // Cleanup before exit
    Cleanup();
    
    return 0;
}