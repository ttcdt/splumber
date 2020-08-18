/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#ifdef DJGPP
#include <io.h>
#include <dir.h>
#endif

#ifdef linux
#define O_BINARY 0
#endif

int _zpck_file = 0;


int main(int argc, char *argv[])
{
    long size;
    int i, o;
    int n, c, m;
    char *ptr;
    FILE *fi;
    FILE *fo;
    char tmp[80];
    int opt;

    if (argc < 3) {
        printf("\nSpace Plumber Packer - Angel Ortega\n\n");
        printf("Usage: xpck [-z] {filename.pck} {filenames...}\n");
        return (1);
    }

    if (argv[1][0] == '-') {
        opt = 2;

        if (strcmp(argv[1], "-z") == 0)
            _zpck_file = 1;
    }
    else
        opt = 1;

    if ((o = open(argv[opt], O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
              S_IREAD | S_IWRITE)) == -1) {
        printf("Error: '%s'\n", argv[1]);
        return (2);
    }

    if (_zpck_file)
        write(o, "<zpck>", 6);
    else
        write(o, "<xpck>", 6);

    for (n = opt + 1; n < argc; n++) {
        /* pasa a minúsculas */
        for (m = 0; argv[n][m]; m++)
            argv[n][m] = tolower(argv[n][m]);

        if ((i = open(argv[n], O_RDONLY | O_BINARY)) == -1)
            printf("Error: '%s'\n", argv[n]);
        else {
            if (_zpck_file) {
                close(i);

                sprintf(tmp, "gzip -c %s", argv[n]);

                if ((fi = popen(tmp, "rb")) == NULL) {
                    perror("gzip error");
                    printf("\tfile: '%s'\n", argv[n]);
                    continue;
                }

                fo = fopen("xpck.tmp", "wb");

                while ((c = fgetc(fi)) != EOF)
                    fputc(c, fo);

                fclose(fo);
                pclose(fi);

                if ((i = open("xpck.tmp", O_RDONLY | O_BINARY)) == -1)
                    printf("Tmp rrror: '%s'\n", argv[n]);
            }

            printf("%s\n", argv[n]);

            size = strlen(argv[n]) + 1;

            /* escribe el tamaño del nombre */
            write(o, &size, sizeof(size));

            /* escribe el nombre */
            write(o, argv[n], size);

            /* calcula el tamaño del fichero */
            size = lseek(i, 0, SEEK_END);
            lseek(i, 0, SEEK_SET);

            /* lo escribe */
            write(o, &size, sizeof(size));

            ptr = (char *) malloc(size);

            if (ptr) {
                read(i, ptr, size);
                write(o, ptr, size);

                free(ptr);
            }

            close(i);
        }
    }

    close(i);
    close(o);

    unlink("xpck.tmp");

    return (0);
}
