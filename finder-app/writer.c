#include <stdio.h>
#include <syslog.h>

int write_file(char *filepath, char *content) {
    FILE *fp;

    syslog(LOG_DEBUG, "Writing %s to %s", content, filepath);

    fp = fopen(filepath, "w");
    if(fp == NULL) {
        printf("cannot open file %s", filepath);
        syslog(LOG_ERR, "cannot open file %s", filepath);
        goto file_error;
    }
    if(fprintf(fp, "%s", content) < 0) {
        printf("cannot write to file %s", filepath);
        syslog(LOG_ERR, "cannot write to file %s", filepath);
        goto file_error;
    }
    fclose(fp);
    return 0;

file_error:
    fclose(fp);
    return 1;
}

int main(int argc, char **argv) {
    openlog(NULL, 0, LOG_USER);
    if(argc != 3) {
        printf("invalid number of argument: expected 2, got %d", argc);
        syslog(LOG_ERR, "invalid number of argument: expected 2, got %d", argc);
        return  1;
    }
    if(write_file(argv[1], argv[2]) != 0) {
        return 1;
    }
    return 0;
}