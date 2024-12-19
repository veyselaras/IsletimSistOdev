// Arka plan sürecini listeye ekleme
void arkaPlanProcessEkle(pid_t pid) {
    arkaplanProcess_t* yeniProcess = malloc(sizeof(arkaplanProcess_t));
    yeniProcess->pid = pid;
    yeniProcess->sonraki = arkaplanListesi;
    arkaplanListesi = yeniProcess;
}
//deneme
// Tüm arka plan süreçlerinin bitmesini beklemek
void arkplanBekle() {
    arkaplanProcess_t* suanki = arkaplanListesi;
    while (suanki) {
        int durum;
        waitpid(suanki->pid, &durum, 0);
        printf("[%d] retval: %d\n", suanki->pid, WEXITSTATUS(durum));
        arkaplanProcess_t* temp = suanki;
        suanki = suanki->sonraki;
        free(temp);
    }
}

//Increment komutunu calistirmak
int execute_increment(const char* input_file) {
    int numara;
    FILE* girdi = stdin;

    if (input_file) {
        girdi = fopen(input_file, "r");
        if (!girdi) {
            fprintf(stderr, "%s giriş dosyası bulunamadı.\n", input_file);
            return -1;
        }
    }

    if (fscanf(girdi, "%d", &numara) != 1) {
        fprintf(stderr, "Geçersiz giriş\n");
        if (input_file) fclose(girdi);
        return -1;
    }

    printf("%d\n", numara + 1);
    fflush(stdout);

    if (input_file) fclose(girdi);
    return 0;
}

//Komutalari calistirmak icin fonksiyon
int execute_command(char** args, char* girisDosyasi, char* cikisDosyasi, int arkaplan) {
    // "increment" komutu için özel işlem
    if (strcmp(args[0], "increment") == 0) {
        return execute_increment(girisDosyasi);
    }

    pid_t pid = fork();
    if (pid == 0) {
        if (girisDosyasi) {
            int dosya = open(girisDosyasi, O_RDONLY);
            if (dosya < 0) {
                fprintf(stderr, "%s giris dosyasi bulunamadi.\n", girisDosyasi);
                exit(1);
            }
            dup2(dosya, STDIN_FILENO);
            close(dosya);
        }
        if (cikisDosyasi) {
            int dosya = open(cikisDosyasi, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (dosya < 0) {
                perror("output file");
                exit(1);
            }
            dup2(dosya, STDOUT_FILENO);
            close(dosya);
        }
        execvp(args[0], args);
        perror("exec");
        fprintf(stderr, "Komut bulunamadi: %s\n", args[0]);
        exit(1);
    }
    else if (pid < 0) {
        perror("fork");
        return -1;
    }
    else {
        if (arkaplan) {
            arkaPlanProcessEkle(pid);
        }
        else {
            int durum;
            waitpid(pid, &durum, 0);
        }
    }
    return 0;
}

//Pipe komutlarini yurutmek icin fonksiyon
int execute_pipe_commands(char** pipeCmd, int pipeSayac, char* globalGirisDosyasi, char* globalCikisDosyasi) {
    int cmdNumara = pipeSayac + 1;
    int i;
    int pipes[2 * pipeSayac];
    for (i = 0; i < pipeSayac; i++) {
        if (pipe(pipes + i * 2) < 0) {
            perror("pipe");
            return -1;
        }
    }

    for (i = 0; i < cmdNumara; i++) {
        char* args[MAX_ARGS];
        char* girisDosyasi = NULL;
        char* cikisDosyasi = NULL;
        int arkaplan = 0;
        char** dummy_pipe = NULL; int dummy_count = 0;
        parse_command(pipeCmd[i], args, &girisDosyasi, &cikisDosyasi, &arkaplan, &dummy_pipe, &dummy_count);

        pid_t pid = fork();
        if (pid == 0) {
            if (i == 0 && globalGirisDosyasi) {
                int dosya = open(globalGirisDosyasi, O_RDONLY);
                if (dosya < 0) {
                    fprintf(stderr, "%s giriş dosyası bulunamadı.\n", globalGirisDosyasi);
                    exit(1);
                }
                dup2(dosya, STDIN_FILENO);
                close(dosya);
            }

            if (i == cmdNumara - 1 && globalCikisDosyasi) {
                int dosya = open(globalCikisDosyasi, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (dosya < 0) {
                    perror("output file");
                    exit(1);
                }
                dup2(dosya, STDOUT_FILENO);
                close(dosya);
            }

            if (i > 0) {
                dup2(pipes[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < cmdNumara - 1) {
                dup2(pipes[i * 2 + 1], STDOUT_FILENO);
            }

            for (int j = 0; j < 2 * pipeSayac; j++) {
                close(pipes[j]);
            }

            if (strcmp(args[0], "increment") == 0) {
                execute_increment(NULL);
                exit(0);
            }

            execvp(args[0], args);
            perror("exec");
            fprintf(stderr, "Komut bulunamadi: %s\n", args[0]);
            exit(1);
        }
        else if (pid < 0) {
            perror("fork");
            return -1;
        }
    }

    for (int j = 0; j < 2 * pipeSayac; j++) {
        close(pipes[j]);
    }

    for (int k = 0; k < cmdNumara; k++) {
        int durum;
        wait(&durum);
    }

    return 0;
}

//Ardasil komutlari yurutmek icin fonksiyon
void execute_sequential_commands(char* line) {
    char* kayitPtr;
    char* komut = strtok_r(line, ";", &kayitPtr);
    while (komut != NULL) {
        char args[MAX_ARGS][MAX_CMD_LEN];
        char* girisDosyasi = NULL;
        char* cikisDosyasi = NULL;
        int arkaplan = 0;
        int pipeSayac = 0;
        char** pipeCmd = NULL;

        parse_command(komut, (char**)args, &girisDosyasi, &cikisDosyasi, &arkaplan, &pipeCmd, &pipeSayac);

        if (pipeSayac > 0 && pipeCmd != NULL) {
            execute_pipe_commands(pipeCmd, pipeSayac, girisDosyasi, cikisDosyasi);
            for (int i = 0; i < pipeSayac + 1; i++) {
                free(pipeCmd[i]);
            }
            free(pipeCmd);
        }
        else {
            if (args[0] != NULL) {
                execute_command((char**)args, girisDosyasi, cikisDosyasi, arkaplan);
            }
        }

        komut = strtok_r(NULL, ";", &kayitPtr);
    }
}

//main fonksiyonumuz
int main() {
    struct sigaction sinyalKontrol;
    sinyalKontrol.sa_handler = sigchld_handler;
    sigemptyset(&sinyalKontrol.sa_mask);
    sinyalKontrol.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sinyalKontrol, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    while (!quit_requested) {
        printf("> ");
        fflush(stdout);

        char line[MAX_CMD_LEN];
        if (fgets(line, MAX_CMD_LEN, stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) continue;

        if (strcmp(line, "quit") == 0) {
            quit_requested = 1;
            break;
        }

        execute_sequential_commands(line);
    }

    arkplanBekle();
    return 0;
}