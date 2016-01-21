#include<stdio.h>

int main(int argc, char *argv[]) {
	FILE *fInput;
	if (argc > 1) {
		fInput = freopen(argv[1], "r", stdin);
		if (fInput == NULL) {
			printf("wc: %s: No such file or directory\n", argv[1]);
			return 0;
		}
	}
	int nLine = 0, nWord = 0, nChar = 0, isLastSpace = 1, nLineChar = 0;
	char cNext;
	while ((cNext = getchar()) != EOF) {
		++nChar;
		++nLineChar;
		if (isspace(cNext)) {
			isLastSpace = 1;
			if (cNext == '\n') {
				++nLine;
				nLineChar = 0;
			}
		} else {
			if (isLastSpace)
				++nWord;
			isLastSpace = 0;
		}
	}
	if (nLineChar)
		++nLine;
	if (fInput == NULL)
		printf("%d\t%d\t%d\n", nLine, nWord, nChar);
	else {
		printf("%d\t%d\t%d\t%s\n", nLine, nWord, nChar, argv[1]);
		fclose(fInput);
	}
    return 0;
}
