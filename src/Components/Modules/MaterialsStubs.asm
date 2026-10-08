.code

IwiVersionStub PROC
	cmp byte ptr [rbp - 17h], 69h
	jne done

	cmp byte ptr [rbp - 16h], 8
	je done

	cmp byte ptr [rbp - 16h], 9

done:
	ret
IwiVersionStub ENDP

END
