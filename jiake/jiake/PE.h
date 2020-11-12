#include <Windows.h>

typedef struct _StubConf
{
	DWORD srcOep;		//��ڵ�
	DWORD textScnRVA;	//�����RVA
	DWORD textScnSize;	//����εĴ�С
	DWORD key;			//������Կ
}StubConf;

struct StubInfo
{
	char* dllbase;			//stub.dll�ļ��ػ�ַ
	DWORD pfnStart;			//stub.dll(start)���������ĵ�ַ
	StubConf* pStubConf;	//stub.dll(g_conf)����ȫ�ֱ����ĵ�ַ
};


//***********************
//PE��Ϣ��ȡ������
//time:2020/11/2
//***********************
PIMAGE_DOS_HEADER GetDosHeader(_In_ char* pBase) {
	return PIMAGE_DOS_HEADER(pBase);
}

PIMAGE_NT_HEADERS GetNtHeader(_In_ char* pBase) {
return PIMAGE_NT_HEADERS(GetDosHeader(pBase)->e_lfanew+(SIZE_T)pBase);
}

PIMAGE_FILE_HEADER GetFileHeader(_In_ char* pBase) {
	return &(GetNtHeader(pBase)->FileHeader);
}

PIMAGE_OPTIONAL_HEADER32 GetOptHeader(_In_ char* pBase) {
	return &(GetNtHeader(pBase)->OptionalHeader);
}

PIMAGE_SECTION_HEADER GetLastSec(_In_ char* pBase) {
	DWORD SecNum = GetFileHeader(pBase)->NumberOfSections;
	PIMAGE_SECTION_HEADER FirstSec = IMAGE_FIRST_SECTION(GetNtHeader(pBase));
	PIMAGE_SECTION_HEADER LastSec = FirstSec + SecNum - 1;
	return LastSec;
}

PIMAGE_SECTION_HEADER GetSecByName(_In_ char* pBase,_In_ const char* name) {
	DWORD Secnum = GetFileHeader(pBase)->NumberOfSections;
	PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(GetNtHeader(pBase));
	char buf[10] = { 0 };
	for (DWORD i = 0; i < Secnum; i++) {
		memcpy_s(buf, 8, (char*)Section[i].Name, 8);
		if (!strcmp(buf, name)) {
				return Section + i;
		}
	}
	return nullptr;
}

//**********************
//���ļ����ؾ��
//time:2020/11/2
//*********************
char* GetFileHmoudle(_In_ const char* path,_Out_opt_ DWORD* nFileSize) {
	//��һ���ļ�������ļ����
	HANDLE hFile = CreateFileA(path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	//����ļ���С
	DWORD FileSize = GetFileSize(hFile, NULL);
	//�����ļ���С������nFileSize
	if(nFileSize)
		*nFileSize = FileSize;
	//����һƬ��СΪFileSize���ڴ沢��ָ��������λ
	char* pFileBuf = new CHAR[FileSize]{ 0 };
	//���ո�������ڴ��������
	DWORD dwRead;
	ReadFile(hFile, pFileBuf, FileSize, &dwRead, NULL);
	CloseHandle(hFile);
	return pFileBuf;
}

//****************
//���봦��
//time:2020/11/5
//****************
int AlignMent(_In_ int size, _In_ int alignment) {
	return (size) % (alignment)==0 ? (size) : ((size) / alignment+1) * (alignment);
}

//*********************
//��������
//time:2020/11/6
//*********************
char* AddSec(_In_ char*& hpe, _In_ DWORD& filesize, _In_ const char* secname, _In_ const int secsize) {
	GetFileHeader(hpe)->NumberOfSections++;
	PIMAGE_SECTION_HEADER pesec = GetLastSec(hpe);
	//�������α�����
	memcpy(pesec->Name, secname, 8);
	pesec->Misc.VirtualSize = secsize;
	pesec->VirtualAddress = (pesec - 1)->VirtualAddress + AlignMent((pesec - 1)->SizeOfRawData,GetOptHeader(hpe)->SectionAlignment);
	pesec->SizeOfRawData = AlignMent(secsize, GetOptHeader(hpe)->FileAlignment);
	pesec->PointerToRawData = AlignMent(filesize,GetOptHeader(hpe)->FileAlignment);
	pesec->Characteristics = 0xE00000E0;
	//����OPTͷӳ���С
	GetOptHeader(hpe)->SizeOfImage = pesec->VirtualAddress + pesec->SizeOfRawData;
	//�����ļ�����
	int newSize = pesec->PointerToRawData + pesec->SizeOfRawData;
	char* nhpe = new char [newSize] {0};
	//���»�����¼������
	memcpy(nhpe, hpe, filesize);
	//����������
	delete hpe;
	filesize = newSize;
	return nhpe;
}

//******************
//�����ļ�
//time:2020/11/6
//******************
void SaveFile(_In_ const char* path, _In_ const char* data, _In_ int FileSize) {
	HANDLE hFile = CreateFileA(
		path,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	DWORD Buf = 0;
	WriteFile(hFile, data, FileSize, &Buf,NULL);
	CloseHandle(hFile);
}

//*******************
//����stub
//time:
//******************
//void StubLoad (_In_ StubInfo* pStub) {
//
//}


void FixStub(DWORD targetDllbase, DWORD stubDllbase,DWORD targetNewScnRva,DWORD stubTextRva )
{
	//�ҵ�stub.dll���ض�λ��
	DWORD dwRelRva = GetOptHeader((char*)stubDllbase)->DataDirectory[5].VirtualAddress;
	IMAGE_BASE_RELOCATION* pRel = (IMAGE_BASE_RELOCATION*)(dwRelRva + stubDllbase);

	//�����ض�λ��
	while (pRel->SizeOfBlock)
	{
		struct TypeOffset
		{
			WORD offset : 12;
			WORD type : 4;

		};
		TypeOffset* pTypeOffset = (TypeOffset*)(pRel + 1);
		DWORD dwCount = (pRel->SizeOfBlock - 8) / 2;	//��Ҫ�ض�λ������
		for (int i = 0; i < dwCount; i++)
		{
			if (pTypeOffset[i].type != 3)
			{
				continue;
			}
			//��Ҫ�ض�λ�ĵ�ַ
			DWORD* pFixAddr = (DWORD*)(pRel->VirtualAddress + pTypeOffset[i].offset + stubDllbase);

			DWORD dwOld;
			//�޸�����Ϊ��д
			VirtualProtect(pFixAddr, 4, PAGE_READWRITE, &dwOld);
			//ȥ��dll��ǰ���ػ�ַ
			*pFixAddr -= stubDllbase;
			//ȥ��Ĭ�ϵĶ���RVA
			*pFixAddr -= stubTextRva;
			//����Ŀ���ļ��ļ��ػ�ַ
			*pFixAddr += targetDllbase;
			//���������εĶ���RVA
			*pFixAddr += targetNewScnRva;
			//�������޸Ļ�ȥ
			VirtualProtect(pFixAddr, 4, dwOld, &dwOld);
		}
		//�л�����һ���ض�λ��
		pRel = (IMAGE_BASE_RELOCATION*)((DWORD)pRel + pRel->SizeOfBlock);
	}

}
//******************
//���ܴ����
//time:
//******************
void Encry(_In_ char* hpe,_In_ StubInfo pstub) {
	//��ȡ������׵�ַ
	BYTE* TargetText = GetSecByName(hpe, ".text")->PointerToRawData + (BYTE*)hpe;
	//��ȡ����δ�С
	DWORD TargetTextSize = GetSecByName(hpe, ".text")->Misc.VirtualSize;
	//���ܴ����
	for (int i = 0; i < TargetTextSize; i++) {
		TargetText[i] ^= 0x15;
	}
	pstub.pStubConf->textScnRVA = GetSecByName(hpe, ".text")->VirtualAddress;
	pstub.pStubConf->textScnSize = TargetTextSize;
	pstub.pStubConf->key = 0x15;
}

void LoadStub(_In_ StubInfo* pstub) {
	pstub->dllbase = (char*)LoadLibraryEx(L"F:\\stubdll.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
	pstub->pfnStart = (DWORD)GetProcAddress((HMODULE)pstub->dllbase, "Start");
	pstub->pStubConf = (StubConf*)GetProcAddress((HMODULE)pstub->dllbase, "g_conf");
}