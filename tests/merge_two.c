#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *file1, *file2, *mergedFile;
    char file1Name[100], file2Name[100], mergedFileName[100];
    char c;

    printf("Enter first filename: ");
    scanf("%s", file1Name);

    printf("Enter second filename: ");
    scanf("%s", file2Name);

    printf("Enter merged output filename: ");
    scanf("%s", mergedFileName);

    // Open files
    file1 = fopen(file1Name, "r");
    if (file1 == NULL)
    {
        printf("Error! Cannot open first file.\n");
        exit(1);
    }

    file2 = fopen(file2Name, "r");
    if (file2 == NULL)
    {
        fclose(file1);
        printf("Error! Cannot open second file.\n");
        exit(1);
    }

    mergedFile = fopen(mergedFileName, "w");
    if (mergedFile == NULL)
    {
        fclose(file1);
        fclose(file2);
        printf("Error! Cannot create merged output file.\n");
        exit(1);
    }

    // Copy contents of first file
    while ((c = fgetc(file1)) != EOF)
    {
        fputc(c, mergedFile);
    }

    // Copy contents of second file
    while ((c = fgetc(file2)) != EOF)
    {
        fputc(c, mergedFile);
    }

    fclose(file1);
    fclose(file2);
    fclose(mergedFile);

    printf("Files merged successfully into %s\n", mergedFileName);

    return 0;
}