#include <Windows.h>
//#include "stdafx.h"
#include <tchar.h>
#include <stdio.h>
#include "PE.h"

int main() {
	//�򿪱��ӿ��ļ�
	char PePath[] = "F:\\Project.exe";
	DWORD PeSize;
	char* PeHmoudle = GetFileHmoudle(PePath,&PeSize);
	//����stub
	StubInfo pstub = { 0 };
	LoadStub(&pstub); 
	
	//���ܴ����
	DWORD textRVA = GetSecByName(PeHmoudle, ".text")->VirtualAddress;
	DWORD textSize = GetSecByName(PeHmoudle, ".text")->Misc.VirtualSize;
	Encry(PeHmoudle,pstub);

	//���������
	char SecName[] = ".fuck";
	char* PeNewHmoudle = AddSec(PeHmoudle, PeSize, SecName, GetSecByName(pstub.dllbase, ".text")->Misc.VirtualSize);
	
	//stub�ض�λ�޸�
	FixStub(GetOptHeader(PeNewHmoudle)->ImageBase,
		(DWORD)pstub.dllbase,
		GetLastSec(PeNewHmoudle)->VirtualAddress,
		GetSecByName(pstub.dllbase,".text")->VirtualAddress);
	auto b = (DWORD*)GetProcAddress((HMODULE)pstub.dllbase, "OriginEntry");
	pstub.pStubConf->srcOep = GetOptHeader(PeNewHmoudle)->AddressOfEntryPoint;  //��ȡԭ��ڵ�
	
	//stub��ֲ
	memcpy(GetLastSec(PeNewHmoudle)->PointerToRawData+ PeNewHmoudle,
		GetSecByName(pstub.dllbase, ".text")->VirtualAddress+pstub.dllbase,
		GetSecByName(pstub.dllbase,".text")->Misc.VirtualSize);
	
	////��ڵ��޸�
	GetOptHeader(PeNewHmoudle)->AddressOfEntryPoint =
		pstub.pfnStart-(DWORD)pstub.dllbase-GetSecByName(pstub.dllbase,".text")->VirtualAddress+GetLastSec(PeNewHmoudle)->VirtualAddress;
	auto a =pstub.pfnStart-(DWORD)pstub.dllbase-GetSecByName(pstub.dllbase,".text")->VirtualAddress+GetLastSec(PeNewHmoudle)->VirtualAddress;
	auto d =GetProcAddress((HMODULE)pstub.dllbase, "OriginEntry");

	//ȥ�����ַ
	GetOptHeader(PeNewHmoudle)->DllCharacteristics &= (~0x40);
	
	//�����ļ�
	SaveFile("F:\\fuck.exe", PeNewHmoudle, PeSize);

	return 0;
}