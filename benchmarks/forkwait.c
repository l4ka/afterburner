int main(int argc, char *argv[])
{
    int i;
    for (i=0; i < 200000; i++)
    {
	int pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) return 0;
	waitpid(pid);
    }
    return 0;
}

