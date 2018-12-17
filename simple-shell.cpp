/*
	Name:        Chowdhury, Monazil Hoque
	Project:     PA-1a (A Simple Shell)
	Instructor:  Feng Chen
	Class:       cs7103_au17
	LogonID:     cs710313
*/

#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<fcntl.h>
#include<unistd.h>
#include<sys/wait.h>
#include<deque>
#include<string>
#include<set>
#include<map>
#include<utility>
#include<iostream>


using namespace std;

//forward declaration
void shell_loop();
string shell_read_line();
char ** shell_split_line(char *line, const char *delim);
int shell_parse_execute(char **args);
int shell_execute(char **args);
int shell_cd(char **args);
int shell_exit(char **args);
int shell_num_of_buildins();
int shell_num_of_specials();
int shell_launch_pipe(char **commands);
int showHistory(char **args);
void addHistory(string command);
int showHistat(char **args);
void addHistat(string command);
int shell_launch_redirection(char *command);

//constants
#define PIPE_DELIM "|"
#define TOKEN_BUFER_SIZE 64
#define TOKEN_DELIM " \t\r\n\a"
#define HISTORY_SIZE 100 //maximum number of command will be stored in history
#define HISTAT_SIZE 10 //maximum number of commands will be stored in histat

//global data structures
deque<string> history;

/*
histat stores (-command_frequency, command) as pair
the sign of frequncy is reversed to store the words in descending order of their frequency
*/
set<pair<int, string> > histat; 

/*
frequncyMap
	key -> typed command
	value -> pair (-command_frequncy, a boolean value that indicates whether the command is in "histat")
*/
map<string, pair<int, bool> > frequencyMap;

const char *buildin_str[] = {
	"history",
	"histat"
};

const char *special_str[] = {
	"cd",
	"exit"
};

int (*buildin_func[]) (char **args) = {
	showHistory,
	showHistat
};

int (*special_func[]) (char **args) = {
	shell_cd,
	shell_exit
};

int main() {
	//todo: save history & histat
	shell_loop();
	//todo: restore history & histat
	return 0;
}

void shell_loop() {

	string line_str;
	char *line = NULL;
	char **commands;
	int status = 0;

	do {
		printf("> ");
		line_str = shell_read_line();

		if(line_str.length() < 1) {
			continue;
		}

		line = (char *) malloc(sizeof(char) * (line_str.length() + 1));
		strcpy(line, line_str.c_str());
		commands = shell_split_line(line, PIPE_DELIM);
		status = shell_parse_execute(commands);
		addHistory(line_str);
		addHistat(line_str);
		free(line);
		free(commands);
	} while(status > 0);
}

string shell_read_line() {

	string line;
	getline(cin, line);
	return line;
}

char ** shell_split_line(char *line, const char *delim = TOKEN_DELIM) {

	int buffer_size = TOKEN_BUFER_SIZE;
	char **tokens = (char **) malloc(sizeof(char*) * buffer_size);
	char *token = strtok(line, delim);
	int index = 0;

	while(token != NULL) {
		tokens[index++] = token;

		if(index == buffer_size) {
			buffer_size += TOKEN_BUFER_SIZE;
			tokens = (char**) realloc(tokens, sizeof(char*) * buffer_size);
			if(tokens == NULL) {
				exit(1);
			}
		}

		token = strtok(NULL, delim);
	}

	tokens[index] = NULL;
	return tokens;
}

int shell_parse_execute(char **commands) {

	if(commands[0] == NULL) {
		return 1;
	}

	if(commands[1] == NULL) {

		if(strchr(commands[0], '>') != NULL || strchr(commands[0], '<') != NULL) {
			return shell_launch_redirection(commands[0]);
		}

		char **args = shell_split_line(commands[0]);
		return shell_execute(args);

	} else {
		return shell_launch_pipe(commands);

	}
}

#define DELIM_REDIRECTION "><"

int shell_launch_redirection(char *command) {

	char command_copy[strlen(command) + 1];
	strcpy(command_copy, command);
	char **splitted_command = shell_split_line(command, DELIM_REDIRECTION);
	char *commandName = splitted_command[0];
	char *input_file = NULL;
	char *output_file = NULL;
	int j = 1;

	//find I/O file name
	for(int i = 0; command_copy[i] && splitted_command[j]; i++) {

		if(command_copy[i] == '<') {

			input_file = splitted_command[j++];

		} else if(command_copy[i] == '>') {
			
			output_file = splitted_command[j++];
		}
	}

	if(input_file == NULL && output_file == NULL) {
		printf("shell: syntex error\n");
		return 1;
	}

	int std_in_copy;
	int std_out_copy;
	int input_file_fd;
	int output_file_fd;

	//redirect STD_IN to input_file
	if(input_file != NULL) {

		std_in_copy = dup(STDIN_FILENO);
		close(STDIN_FILENO);
		input_file_fd = open(input_file, O_RDONLY);

		if(input_file_fd < 0) {
			perror(input_file);
			dup2(std_in_copy, STDIN_FILENO);
			close(std_in_copy);
			return 1;
		}
	}

	//redirect STD_OUT to output_file
	if(output_file != NULL) {

		std_out_copy = dup(STDOUT_FILENO);
		close(STDOUT_FILENO);
		output_file_fd = open(output_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		if(output_file_fd < 0) {
			perror(output_file);
			dup2(std_out_copy, STDOUT_FILENO);
			close(std_out_copy);
			return 1;
		}
	}

	pid_t pid = fork();
	if(pid == 0) {

		char **command_args = shell_split_line(commandName);

		for(int i = 0; i < shell_num_of_buildins(); i++) {

			if(strcmp(buildin_str[i], command_args[0]) == 0) {

				buildin_func[i](command_args);
				exit(EXIT_SUCCESS);
			}
		}

		if(execvp(command_args[0], command_args) == -1) {
			perror(command_args[0]);
		}

		exit(EXIT_FAILURE);
	}

	//restore STD_IN
	if(input_file != NULL) {

		close(input_file_fd);
		dup2(std_in_copy, 0);
		close(std_in_copy);
	}

	//restore STD_OUT
	if(output_file != NULL) {

		close(output_file_fd);
		dup2(std_out_copy, 1);
		close(std_out_copy);
	}

	int status;
	waitpid(pid, &status, WUNTRACED);
	return 1;
}

int getLength(char **stringArray) {

	int length = 0;
	while(stringArray[length]) {
		length++;
	}
	return length;
}

int shell_launch_pipe(char **commands) {

	int num_of_commands = getLength(commands);
	int num_of_pipes = num_of_commands - 1;
	int pipe_descriptor[num_of_pipes][2];

	for(int i = 0; i < num_of_pipes; i++) {
		pipe(pipe_descriptor[i]);
	}

	pid_t child_id[num_of_commands];

	for(int i = 0; commands[i] != NULL; i++) {

		child_id[i] = fork();

		if(child_id[i] == 0) {

			if(i != num_of_commands - 1) {
				dup2(pipe_descriptor[i][1], STDOUT_FILENO);
			}

			for(int j = i + 1; j < num_of_pipes; j++) {
				close(pipe_descriptor[j][1]);
			}

			if(i > 0) {
				dup2(pipe_descriptor[i-1][0], STDIN_FILENO);
			}

			for(int j = 0; j < num_of_pipes; j++) {

				if(j != i - 1) {
					close(pipe_descriptor[j][0]);
				}
			}

			char **args = shell_split_line(commands[i]);

			if(execvp(args[0], args) == -1) {
				perror(args[0]);
			}

			exit(EXIT_FAILURE);

		} else {

			if(i < num_of_pipes) {
				close(pipe_descriptor[i][1]);
			}

			int status;
			waitpid(child_id[i], &status, WUNTRACED);
		}
	}

	for(int i = 0; i < num_of_pipes; i++) {
		close(pipe_descriptor[i][0]);
	}

	return 1;
}

int shell_execute(char **args) {

	//if command matches with any special command then execute it in parent process
	for(int i = 0; i < shell_num_of_specials(); i++) {

		if(strcmp(special_str[i], args[0]) == 0) {
				
			return special_func[i](args);
		}
	}

	pid_t pid, wpid;
	int status;
	pid = fork();

	if(pid == 0) {
		//child process

		for(int i = 0; i < shell_num_of_buildins(); i++) {

			if(strcmp(buildin_str[i], args[0]) == 0) {

				buildin_func[i](args);
				exit(EXIT_SUCCESS);
			}
		}

		if(execvp(args[0], args) == -1) {

			perror(args[0]);
		}

		exit(EXIT_FAILURE);

	} else if(pid < 0) {
		perror(args[0]);

	} else {

		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while(!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	return 1;
}

int shell_num_of_buildins() {

	static int size = sizeof(buildin_str)/sizeof(char *);
	return size;
}

int shell_num_of_specials() {

	static int size = sizeof(special_str)/sizeof(char *);
	return size;
}

int shell_cd(char **args) {

	if(args[1] == NULL) {
		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
	} else {
		if(chdir(args[1]) != 0) {
			perror(args[0]);
		}
	}
	return 1;
}

int shell_exit(char **args) {
	return 0;
}

// #define COMMAND_SIZE 30
// #define NO_OF_COMMANDS 10
// char** split_command(char *line) {
//     char **commands = (char**) malloc(sizeof(char*) * NO_OF_COMMANDS);
//     char *command = NULL;
//     int commands_index = 0;
//     int index, i;
//     for(i = 0; line[i] && line[i] != '\n'; i++) {

//         if(command == NULL) {
//             command = (char*) malloc(sizeof(char) * COMMAND_SIZE);
//             index = 0;
//         }

//         if(line[i] == '|') {
//             command[index] = '\0';
//             commands[commands_index++] = command;
//             command = NULL;
//         } else {
//             command[index++] = line[i];
//         }
//     }

//     if(i > 0) {
//         command[index] = '\0';
//         commands[commands_index++] = command;
//     }
//     commands[commands_index] = '\0';
//     return commands;
// }

void addHistory(string command) {

	while(history.size() >= HISTORY_SIZE) {
		history.pop_back();
	}

	history.push_front(command);
}

void addHistat(string command) {

	int frequency = -1;
	bool isInHistat = false;

	if(frequencyMap.find(command) != frequencyMap.end()) {
		pair<int, bool> value = frequencyMap[command];
		frequency = value.first - 1;

		if(value.second) { //command is in histat
			histat.erase(make_pair(value.first, command));
			histat.insert(make_pair(frequency, command));
			isInHistat = true;
		} else { //command not in histat

			if(histat.size() < HISTAT_SIZE) { //space available in histat
				histat.insert(make_pair(frequency, command));
				isInHistat = true;
			} else { //if possible, replace least frequent command with this new command
				pair<int, string> leastFrequentCommand = (*(histat.rbegin()));

				if(frequency < leastFrequentCommand.first) {
					histat.erase(leastFrequentCommand);
					frequencyMap[leastFrequentCommand.second] = make_pair(leastFrequentCommand.first, false);
					histat.insert(make_pair(frequency, command));
					isInHistat = true;
				}
			}
		}

	} else {

		if(histat.size() < HISTAT_SIZE) {
			histat.insert(make_pair(-1, command));
			isInHistat = true;
		}
	}

	frequencyMap[command] = make_pair(frequency, isInHistat);
}

int showHistory(char **args) {

	deque<string>::iterator it;
	for(it = history.begin(); it != history.end(); it++) {
		printf("%s\n", (*it).c_str());
	}
	return 1;
}

void printLine() {
	printf("------------------------------------------------------------\n");
}

int showHistat(char **args) {

	printLine();
	printf("%-50s %8s\n", "command", "frequency");
	printLine();
	set<pair<int, string> >::iterator it;
	for(it = histat.begin(); it != histat.end(); it++) {
		printf("%-50s %5d\n", (*it).second.c_str(), -(*it).first);
	}
	return 1;
}