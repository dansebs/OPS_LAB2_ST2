#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[]){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <p> <t> <prob1> [<prob2> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int p = atoi(argv[1]);
    int t = atoi(argv[2]);
    int num_students = argc - 3;

    if(p < 1 || p > 10 || t < 1 || t > 10 ){
        fprintf(stderr, "Invalid value for p or t. p (1-10), t (1-10).\n");
        return EXIT_FAILURE;
    }

    pid_t student_pids[num_students];

    for(int i = 0; i < num_students; i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("fork");
            return EXIT_FAILURE;
        }else if(pid == 0){
            int probability = atoi(argv[3+i]);
            if(probability < 0 || probability > 100){
                fprintf(stderr, "Invalid probability value: %d\n", probability);
                exit(EXIT_FAILURE);
            }
            printf("Student[%d,%d]: probability = %d%%\n",i,getpid(),probability);
            exit(EXIT_SUCCESS);
        }else{
            student_pids[i] = pid;
        }
    }

    printf("Parent waiting for students to complete...\n");

    while (wait(NULL) != -1 || errno != ECHILD);

    printf("All students have exited.\n");

    return EXIT_SUCCESS;

}
