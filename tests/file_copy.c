#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *sourceFile, *destFile;
    char sourceFilename[100], destFilename[100];
    char c;

    printf("Enter source filename: ");
    scanf("%s", sourceFilename);

    printf("Enter destination filename: ");
    scanf("%s", destFilename);

    // Open source file in read mode
    sourceFile = fopen(sourceFilename, "r");
    if (sourceFile == NULL)
    {
        printf("Error! Source file cannot be opened.\n");
        exit(1);
    }

    // Open destination file in write mode
    destFile = fopen(destFilename, "w");
    if (destFile == NULL)
    {
        fclose(sourceFile);
        printf("Error! Destination file cannot be created.\n");
        exit(1);
    }

    // Copy contents from source to destination
    while ((c = fgetc(sourceFile)) != EOF)
    {
        fputc(c, destFile);
    }

    fclose(sourceFile);
    fclose(destFile);

    printf("File copied successfully.\n");

    return 0;
}