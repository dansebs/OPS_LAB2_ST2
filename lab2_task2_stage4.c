#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define MAX_PENDING 100
#define STUDENT_SIGNAL (SIGRTMIN)

static volatile pid_t pending_pids[MAX_PENDING];
static volatile int pending_count = 0;

void teacher_handle_student_signal(int sig, siginfo_t *info, void *context) {
    if (pending_count < MAX_PENDING) {
        pending_pids[pending_count++] = info->si_pid;
    } else {
        fprintf(stderr, "Too many pending signals!\n");
    }
}

void handle_sigchld(int sig) {
    // Just a dummy handler to interrupt pause()
}

void student_handle_sigusr2(int sig) {
    // No action needed; just break sigsuspend
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <p> <t> <prob1> [<prob2> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int p = atoi(argv[1]); // number of parts
    int t = atoi(argv[2]); // each part takes t*100ms
    int num_students = argc - 3;

    if (p < 1 || p > 10 || t < 1 || t > 10) {
        fprintf(stderr, "Invalid value for p or t. p (1-10), t (1-10).\n");
        return EXIT_FAILURE;
    }

    srand((unsigned int) time(NULL));

    // Set up teacher's signal handler for student signals (STUDENT_SIGNAL)
    struct sigaction sa;
    sa.sa_sigaction = teacher_handle_student_signal;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(STUDENT_SIGNAL, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    // Set up SIGCHLD handler for the teacher
    struct sigaction sa_chld;
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
        perror("sigaction SIGCHLD");
        return EXIT_FAILURE;
    }

    pid_t student_pids[num_students];
    int student_issues[num_students]; // Array to store issues for each student
    for (int i = 0; i < num_students; i++) {
        student_issues[i] = 0; // Initialize issues count
    }

    // Fork students
    for (int i = 0; i < num_students; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return EXIT_FAILURE;
        } else if (pid == 0) {
            // Child (student)
            int probability = atoi(argv[3 + i]);
            if (probability < 0 || probability > 100) {
                fprintf(stderr, "Invalid probability value: %d\n", probability);
                exit(EXIT_FAILURE);
            }

            struct sigaction student_sa;
            student_sa.sa_handler = student_handle_sigusr2;
            sigemptyset(&student_sa.sa_mask);
            student_sa.sa_flags = 0;
            sigaction(SIGUSR2, &student_sa, NULL);

            // Block SIGUSR2
            sigset_t block_mask, old_mask;
            sigemptyset(&block_mask);
            sigaddset(&block_mask, SIGUSR2);
            sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

            printf("Student[%d,%d] has started doing task!\n", i, getpid());

            int total_issues = 0;
            for (int part = 1; part <= p; part++) {
                printf("Student[%d,%d] is starting doing part %d of %d!\n",
                       i, getpid(), part, p);

                // Perform the part
                for (int time_slice = 0; time_slice < t; time_slice++) {
                    usleep(100 * 1000); // 100ms
                    if (rand() % 100 < probability) {
                        total_issues++;
                        printf("Student[%d,%d] has issue (%d) doing task!\n",
                               i, getpid(), total_issues);
                        usleep(50 * 1000); // Additional 50ms for issue
                    }
                }

                printf("Student[%d,%d] has finished part %d of %d!\n",
                       i, getpid(), part, p);

                // Signal the teacher
                kill(getppid(), STUDENT_SIGNAL);

                // Wait for teacher acceptance (SIGUSR2)
                sigset_t empty_mask;
                sigemptyset(&empty_mask);
                sigsuspend(&empty_mask);
            }

            printf("Student[%d,%d] has completed the task having %d issues!\n",
                   i, getpid(), total_issues);

            exit(total_issues);
        } else {
            // Parent (teacher)
            student_pids[i] = pid;
        }
    }

    int students_finished = 0;
    while (students_finished < num_students) {
        pause(); // Wait for signals

        // Handle all pending students
        while (pending_count > 0) {
            pid_t student_pid = pending_pids[--pending_count];
            printf("Teacher has accepted solution of student [%d].\n", student_pid);
            kill(student_pid, SIGUSR2);
        }

        // Check if any students have finished
        int status;
        pid_t w;
        while ((w = waitpid(-1, &status, WNOHANG)) > 0) {
            // Find which student this PID belongs to
            for (int i = 0; i < num_students; i++) {
                if (student_pids[i] == w) {
                    // WEXITSTATUS gives the student's issues count
                    int issues = WEXITSTATUS(status);
                    student_issues[i] = issues;
                    students_finished++;
                    break;
                }
            }
        }

        // If all students are done, break out
        if (students_finished == num_students) {
            break;
        }
    }

    printf("All students have exited.\n");

    // Stage 4: Print statistics
    // Calculate total issues
    int total_issues = 0;
    for (int i = 0; i < num_students; i++) {
        total_issues += student_issues[i];
    }

    // Print the table
    printf("\nNo. | Student ID | Issue count\n");
    for (int i = 0; i < num_students; i++) {
        printf("%3d | %10d | %d\n", i+1, student_pids[i], student_issues[i]);
    }
    printf("Total issues: %d\n", total_issues);

    return EXIT_SUCCESS;
}
