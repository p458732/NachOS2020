#include "syscall.h"
int arr [2000];
main()
        {
                int     n , temp;
		
                for (n=20;n<=100;n++){
			arr[n] = n;
			temp +=arr[n];
                        PrintInt(temp);
		}
		return 0;
        }
