#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

volatile sig_atomic_t signals_received = 0;

void handle_sigusr1(int sig){
    signals_received++;
    printf("Teacher: received signal from student.\n");
}

int main(int argc, char *argv[]){
    if(argc<4){
        fprintf(stderr, "Usage: %s <p> <t> <prob1> [<prob2> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int p = atoi(argv[1]);
    int t = atoi(argv[2]);
    int num_students = argc - 3;

    if (p < 1 || p > 10 || t < 1 || t > 10) {
        fprintf(stderr, "Invalid value for p or t. p (1-10), t (1-10).\n");
        return EXIT_FAILURE;
    }

    pid_t student_pids[num_students];

    struct sigaction sa;

    sa.sa_handler = handle_sigusr1;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGUSR1,&sa,NULL)<0){
        perror("sigaction");
        return EXIT_FAILURE;
    }

    for(int i = 0; i < num_students; i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("fork");
            return EXIT_FAILURE;
        }else if(pid == 0){
            int probability = atoi(argv[3 + i]);
            if(probability < 0 || probability > 100){
                fprintf(stderr,"Invalid probability value: %d\n", probability);
                exit(EXIT_FAILURE);
            }

            printf("Student[%d,%d]: Probability = %d%%\n", i, getpid(), probability);

            int total_issues = 0;
            for(int part = 1; part<= p; part++){
                printf("Student[%d,%d]: Starting part %d of %d!\n", i, getpid(),part, p);

                for(int time_slice = 0; time_slice < t; time_slice++){
                    usleep(100 * 1000);
                    if(rand() % 100 < probability){
                        total_issues++;
                        printf("Student[%d,%d]: Issue encountered (%d) during task!\n",i, getpid(),total_issues);
                        usleep(50 * 1000);
                    }
                }

                printf("Student[%d,%d]: Finished part %d of %d!\n", i, getpid(),part, p);
                kill(getppid(),SIGUSR1);
            }

            printf("Student[%d,%d]: Completed task with %d issues!\n",i,getpid(),total_issues);
            exit(total_issues);
        }else{
            student_pids[i] = pid;
        }
    }

    while (wait(NULL) != -1 || errno != ECHILD);

    printf("All students have exited.\n");
    return EXIT_SUCCESS;
}
