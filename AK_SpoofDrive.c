﻿#include "AK_Main.h"

UNICODE_STRING DriverRegistryPath;

PDRIVER_DISPATCH RealDiskDeviceControl = NULL;
PDRIVER_OBJECT DiskDriver = NULL;

#define MAX_HD_COUNT    10
#define SN_LEN          20

typedef struct _SerialNumbers
{
	UCHAR DiskSerial[SN_LEN];
	UCHAR ChangeTo[SN_LEN];
}SerialNumbers, *PSerialNumbers;

typedef struct _SNInfo
{
	ULONG Count;
	SerialNumbers SNS[MAX_HD_COUNT];
}SNInfo, *PSNInfo;


extern NTKERNELAPI
NTSTATUS
ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN PACCESS_STATE PassedAccessState,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_TYPE ObjectType,
	IN KPROCESSOR_MODE AccessMode,
	IN OUT PVOID ParseContext,
	OUT PVOID * Object
);

extern POBJECT_TYPE *IoDriverObjectType;

#define SMART_RCV_DRIVE_DATA \
  CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define  DFP_SEND_DRIVE_COMMAND   \
  CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define  DFP_RECEIVE_DRIVE_DATA   \
  CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


#define  IDE_ATA_IDENTIFY           0xEC

typedef struct _IDINFO
{
	USHORT  wGenConfig;     // WORD 0: 基本信息字
	USHORT  wNumCyls;     // WORD 1: 柱面数
	USHORT  wReserved2;     // WORD 2: 保留
	USHORT  wNumHeads;     // WORD 3: 磁头数
	USHORT  wReserved4;        // WORD 4: 保留
	USHORT  wReserved5;        // WORD 5: 保留
	USHORT  wNumSectorsPerTrack;  // WORD 6: 每磁道扇区数
	USHORT  wVendorUnique[3];   // WORD 7-9: 厂家设定值
	CHAR    sSerialNumber[20];   // WORD 10-19:序列号
	USHORT  wBufferType;    // WORD 20: 缓冲类型
	USHORT  wBufferSize;    // WORD 21: 缓冲大小
	USHORT  wECCSize;     // WORD 22: ECC校验大小
	CHAR    sFirmwareRev[8];   // WORD 23-26: 固件版本
	CHAR    sModelNumber[40];   // WORD 27-46: 内部型号
	USHORT  wMoreVendorUnique;   // WORD 47: 厂家设定值
	USHORT  wReserved48;    // WORD 48: 保留
	struct {
		USHORT  reserved1 : 8;
		USHORT  DMA : 1;     // 1=支持DMA
		USHORT  LBA : 1;     // 1=支持LBA
		USHORT  DisIORDY : 1;    // 1=可不使用IORDY
		USHORT  IORDY : 1;    // 1=支持IORDY
		USHORT  SoftReset : 1;   // 1=需要ATA软启动
		USHORT  Overlap : 1;    // 1=支持重叠操作
		USHORT  Queue : 1;    // 1=支持命令队列
		USHORT  InlDMA : 1;    // 1=支持交叉存取DMA
	} wCapabilities;     // WORD 49: 一般能力
	USHORT  wReserved1;     // WORD 50: 保留
	USHORT  wPIOTiming;     // WORD 51: PIO时序
	USHORT  wDMATiming;     // WORD 52: DMA时序
	struct {
		USHORT  CHSNumber : 1;   // 1=WORD 54-58有效
		USHORT  CycleNumber : 1;   // 1=WORD 64-70有效
		USHORT  UnltraDMA : 1;   // 1=WORD 88有效
		USHORT  reserved : 13;
	} wFieldValidity;     // WORD 53: 后续字段有效性标志
	USHORT  wNumCurCyls;    // WORD 54: CHS可寻址的柱面数
	USHORT  wNumCurHeads;    // WORD 55: CHS可寻址的磁头数
	USHORT  wNumCurSectorsPerTrack;  // WORD 56: CHS可寻址每磁道扇区数
	USHORT  wCurSectorsLow;    // WORD 57: CHS可寻址的扇区数低位字
	USHORT  wCurSectorsHigh;   // WORD 58: CHS可寻址的扇区数高位字
	struct {
		USHORT  CurNumber : 8;   // 当前一次性可读写扇区数
		USHORT  Multi : 1;    // 1=已选择多扇区读写
		USHORT  reserved1 : 7;
	} wMultSectorStuff;     // WORD 59: 多扇区读写设定
	ULONG  dwTotalSectors;    // WORD 60-61: LBA可寻址的扇区数
	USHORT  wSingleWordDMA;    // WORD 62: 单字节DMA支持能力
	struct {
		USHORT  Mode0 : 1;    // 1=支持模式0 (4.17Mb/s)
		USHORT  Mode1 : 1;    // 1=支持模式1 (13.3Mb/s)
		USHORT  Mode2 : 1;    // 1=支持模式2 (16.7Mb/s)
		USHORT  Reserved1 : 5;
		USHORT  Mode0Sel : 1;    // 1=已选择模式0
		USHORT  Mode1Sel : 1;    // 1=已选择模式1
		USHORT  Mode2Sel : 1;    // 1=已选择模式2
		USHORT  Reserved2 : 5;
	} wMultiWordDMA;     // WORD 63: 多字节DMA支持能力
	struct {
		USHORT  AdvPOIModes : 8;   // 支持高级POI模式数
		USHORT  reserved : 8;
	} wPIOCapacity;      // WORD 64: 高级PIO支持能力
	USHORT  wMinMultiWordDMACycle;  // WORD 65: 多字节DMA传输周期的最小值
	USHORT  wRecMultiWordDMACycle;  // WORD 66: 多字节DMA传输周期的建议值
	USHORT  wMinPIONoFlowCycle;   // WORD 67: 无流控制时PIO传输周期的最小值
	USHORT  wMinPOIFlowCycle;   // WORD 68: 有流控制时PIO传输周期的最小值
	USHORT  wReserved69[11];   // WORD 69-79: 保留
	struct {
		USHORT  Reserved1 : 1;
		USHORT  ATA1 : 1;     // 1=支持ATA-1
		USHORT  ATA2 : 1;     // 1=支持ATA-2
		USHORT  ATA3 : 1;     // 1=支持ATA-3
		USHORT  ATA4 : 1;     // 1=支持ATA/ATAPI-4
		USHORT  ATA5 : 1;     // 1=支持ATA/ATAPI-5
		USHORT  ATA6 : 1;     // 1=支持ATA/ATAPI-6
		USHORT  ATA7 : 1;     // 1=支持ATA/ATAPI-7
		USHORT  ATA8 : 1;     // 1=支持ATA/ATAPI-8
		USHORT  ATA9 : 1;     // 1=支持ATA/ATAPI-9
		USHORT  ATA10 : 1;    // 1=支持ATA/ATAPI-10
		USHORT  ATA11 : 1;    // 1=支持ATA/ATAPI-11
		USHORT  ATA12 : 1;    // 1=支持ATA/ATAPI-12
		USHORT  ATA13 : 1;    // 1=支持ATA/ATAPI-13
		USHORT  ATA14 : 1;    // 1=支持ATA/ATAPI-14
		USHORT  Reserved2 : 1;
	} wMajorVersion;     // WORD 80: 主版本
	USHORT  wMinorVersion;    // WORD 81: 副版本
	USHORT  wReserved82[6];    // WORD 82-87: 保留
	struct {
		USHORT  Mode0 : 1;    // 1=支持模式0 (16.7Mb/s)
		USHORT  Mode1 : 1;    // 1=支持模式1 (25Mb/s)
		USHORT  Mode2 : 1;    // 1=支持模式2 (33Mb/s)
		USHORT  Mode3 : 1;    // 1=支持模式3 (44Mb/s)
		USHORT  Mode4 : 1;    // 1=支持模式4 (66Mb/s)
		USHORT  Mode5 : 1;    // 1=支持模式5 (100Mb/s)
		USHORT  Mode6 : 1;    // 1=支持模式6 (133Mb/s)
		USHORT  Mode7 : 1;    // 1=支持模式7 (166Mb/s) ???
		USHORT  Mode0Sel : 1;    // 1=已选择模式0
		USHORT  Mode1Sel : 1;    // 1=已选择模式1
		USHORT  Mode2Sel : 1;    // 1=已选择模式2
		USHORT  Mode3Sel : 1;    // 1=已选择模式3
		USHORT  Mode4Sel : 1;    // 1=已选择模式4
		USHORT  Mode5Sel : 1;    // 1=已选择模式5
		USHORT  Mode6Sel : 1;    // 1=已选择模式6
		USHORT  Mode7Sel : 1;    // 1=已选择模式7
	} wUltraDMA;      // WORD 88:  Ultra DMA支持能力
	USHORT    wReserved89[167];   // WORD 89-255
} IDINFO, *PIDINFO;

char Hex(WCHAR wch)
{
	if (wch <= '9' && wch >= '0') {
		return wch - '0';
	}

	if (wch <= 'F' && wch >= 'A') {
		return wch - 'A' + 0xA;
	}

	if (wch <= 'f' && wch >= 'a') {
		return wch - 'a' + 0xa;
	}
	return 0;
}

VOID ToHexStr(UCHAR* bytes, ULONG len, WCHAR* buf)
{
	ULONG i = 0;
	UCHAR m, n;
	for (i = 0; i < len; ++i) {
		m = bytes[i] / 0x10;
		n = bytes[i] % 0x10;
		buf[i * 2] = m > 9 ? ('A' + m - 0xa) : ('0' + m);
		buf[i * 2 + 1] = n > 9 ? ('A' + n - 0xa) : ('0' + n);
	}
}

ULONG GetIndex(SNInfo* sns, UCHAR* sn)
{
	ULONG i;
	for (i = 0; i < sns->Count; ++i)
	{
		if (memcmp(sns->SNS[i].DiskSerial, sn, SN_LEN) == 0) {
			return i;
		}
	}
	return -1;
}

BOOLEAN GetSNInfo(SNInfo* info)
{
	OBJECT_ATTRIBUTES attr;
	ULONG result;
	HANDLE hReg = NULL;
	NTSTATUS status;
	PKEY_VALUE_PARTIAL_INFORMATION pvpi = NULL;
	ULONG i = 0, j = 0;
	UNICODE_STRING valueStr;
	WCHAR valueBuf[4];
	ULONG ulSize;
	PWCHAR wptr;

	DPRINT("[br]opening registry [%wZ]\n", DriverRegistryPath);
	InitializeObjectAttributes(&attr, &DriverRegistryPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
	status = ZwOpenKey(&hReg, KEY_QUERY_VALUE, &attr);
	if (!NT_SUCCESS(status)) {
		DPRINT("[br]Failed to open registry [%wZ]\n", DriverRegistryPath);
		return FALSE;
	}

	do
	{
		valueBuf[0] = 's';
		valueBuf[1] = 'n';
		valueBuf[2] = '0';
		valueBuf[3] = '\0';
		RtlInitUnicodeString(&valueStr, valueBuf);

		for (i = 0; i < MAX_HD_COUNT; ++i)
		{
			valueBuf[2] = '0' + (WCHAR)i;
			status = ZwQueryValueKey(hReg, &valueStr, KeyValuePartialInformation, NULL, 0, &ulSize);
			if (STATUS_OBJECT_NAME_NOT_FOUND == status || ulSize == 0) {
				DPRINT("[br]Access the registry key [%ws] -> Query size failed！\n", valueStr.Buffer);
				break;
			}
			pvpi = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool(NonPagedPool, ulSize);
			status = ZwQueryValueKey(hReg, &valueStr, KeyValuePartialInformation, pvpi, ulSize, &result);
			if (!NT_SUCCESS(status)) {
				ExFreePool(pvpi);
				DPRINT("[br]Access the registry key [%ws] -> Query key value failed！\n", valueStr.Buffer);
				break;
			}
			DPRINT("[br]%d, %ws\n", pvpi->DataLength, pvpi->Data);
			if (pvpi->DataLength != 164) {
				ExFreePool(pvpi);
				DPRINT("[br]Access the registry key[%ws]->malformed！\n", valueStr.Buffer);
				break;
			}
			wptr = (PWCHAR)pvpi->Data;
			for (j = 0; j < 20; ++j)
			{
				info->SNS[i].DiskSerial[j] = Hex(wptr[j * 2]) * 0x10 + Hex(wptr[j * 2 + 1]);
				info->SNS[i].ChangeTo[j] = Hex(wptr[(20 * 2 + 1) + j * 2]) * 0x10 + Hex(wptr[(20 * 2 + 1) + j * 2 + 1]);
			}
			DPRINT("[br]%s\n", info->SNS[i].DiskSerial);
			DPRINT("[br]%s\n", info->SNS[i].ChangeTo);
			ExFreePool(pvpi);
		}
		info->Count = i;

	} while (0);

	if (hReg) {
		ZwClose(hReg);
	}
	return TRUE;
}

NTSTATUS HookedDiskDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG                  ctrlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS               status;
	ULONG                  i = 0, j = 0;
	IDINFO*                info = NULL;

	PSENDCMDINPARAMS cmdInParameters = ((PSENDCMDINPARAMS)Irp->AssociatedIrp.SystemBuffer);
	ULONG            controlCode = 0;
	PSRB_IO_CONTROL  srbControl;
	ULONG_PTR        buffer;
	PDEVICE_EXTENSION      deviceExtension = DeviceObject->DeviceExtension;
	PCDB                   cdb;
	KEVENT                 event;
	IO_STATUS_BLOCK        ioStatus;
	ULONG                  length;
	PIRP                   irp2;
	SNInfo                 snInfo;
	PUCHAR                      InputBuffer;
	PUCHAR                      OutputBuffer;
	ULONG                       InputBufferLength, OutputBufferLength;

	DPRINT("[br]HookedDiskDeviceControl\t PID:%d CTRL: 0x%08X  SMART 0x%08X IOCTL_STORAGE_QUERY_PROPERTY 0x%08X\n", PsGetCurrentProcessId_D(), ctrlCode, SMART_RCV_DRIVE_DATA, IOCTL_STORAGE_QUERY_PROPERTY);



	ULONG returnlen;
	POBJECT_NAME_INFORMATION *DName = RtlAllocateMemoryV(1024);
	KphQueryNameObject(DeviceObject->DriverObject, DName, 512, &returnlen);
	DPRINT("Device Handle: Name:%wZ\n", DName);
	RtlFreeMemoryV(DName);


	char realdriverserial[128] = { 0 };


	InputBuffer = OutputBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
	InputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	OutputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

	if (ctrlCode == IOCTL_STORAGE_QUERY_PROPERTY)
	{
		if (GetDiskSN(DeviceObject, &realdriverserial)) {
			DPRINT("Real Serial: %s\n", realdriverserial);
		}

		if (OutputBufferLength < sizeof(STORAGE_DESCRIPTOR_HEADER) || InputBufferLength < sizeof(STORAGE_PROPERTY_QUERY))
		{
			return RealDiskDeviceControl(DeviceObject, Irp);
		}


		STORAGE_PROPERTY_QUERY *storagequery = (STORAGE_PROPERTY_QUERY *)InputBuffer;
		if (storagequery->PropertyId != StorageDeviceProperty || storagequery->QueryType != PropertyStandardQuery)
		{
			return RealDiskDeviceControl(DeviceObject, Irp);
		}


		if (OutputBufferLength >= 500)
		{
			STORAGE_DEVICE_DESCRIPTOR *a = (STORAGE_DEVICE_DESCRIPTOR*)OutputBuffer;// RtlAllocateMemoryV(500);
			
			a->Size = 500;
			a->SerialNumberOffset = 250;
			//strcpy(a + 250, "1234567890");
			memcpy((PUCHAR)Irp->AssociatedIrp.SystemBuffer + 250, HDSerial, strlen(HDSerial));
			//memcpy(a + sizeof(STORAGE_DEVICE_DESCRIPTOR) + 1, "1234567890", 10);
			Irp->IoStatus.Information = 500;
		}
		else
		{
			STORAGE_DESCRIPTOR_HEADER *b = (STORAGE_DESCRIPTOR_HEADER*)OutputBuffer;
			b->Size = 500;
			Irp->IoStatus.Information = sizeof(STORAGE_DESCRIPTOR_HEADER);
		}
		

		//RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer, (PVOID)a, 500);
		//Irp->IoStatus.Information = 500;

		//RtlFreeMemoryV(a);

		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Irp->IoStatus.Status;
	}
	

	if (ctrlCode != SMART_RCV_DRIVE_DATA) {
		return RealDiskDeviceControl(DeviceObject, Irp);
	}

	// 以下的代码都是来自于reactos/drivers/storage/class/disk.c
	do
	{
		if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
			(sizeof(SENDCMDINPARAMS) - 1)) {
			status = STATUS_INVALID_PARAMETER;
			break;

		}
		else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
			(sizeof(SENDCMDOUTPARAMS) + 512 - 1)) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		//
		// Create notification event object to be used to signal the
		// request completion.
		//

		KeInitializeEvent(&event, NotificationEvent, FALSE);

		if (cmdInParameters->irDriveRegs.bCommandReg == ID_CMD) {

			length = IDENTIFY_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
			controlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;

		}
		else if (cmdInParameters->irDriveRegs.bCommandReg == SMART_CMD) {
			switch (cmdInParameters->irDriveRegs.bFeaturesReg) {
			case READ_ATTRIBUTES:
				controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS;
				length = READ_ATTRIBUTE_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
				break;
			case READ_THRESHOLDS:
				controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS;
				length = READ_THRESHOLD_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
				break;
			default:
				status = STATUS_INVALID_PARAMETER;
				break;
			}
		}
		else {

			status = STATUS_INVALID_PARAMETER;
		}

		if (controlCode == 0) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		srbControl = ExAllocatePool(NonPagedPool,
			sizeof(SRB_IO_CONTROL) + length);

		if (!srbControl) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		// fill in srbControl fields
		//

		srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlMoveMemory(srbControl->Signature, "SCSIDISK", 8);
		srbControl->Timeout = deviceExtension->TimeOutValue;
		srbControl->Length = length;
		srbControl->ControlCode = controlCode;

		//
		// Point to the 'buffer' portion of the SRB_CONTROL
		//

		buffer = (ULONG_PTR)srbControl + srbControl->HeaderLength;

		//
		// Ensure correct target is set in the cmd parameters.
		//

		cmdInParameters->bDriveNumber = deviceExtension->TargetId;

		//
		// Copy the IOCTL parameters to the srb control buffer area.
		//

		RtlMoveMemory((PVOID)buffer, Irp->AssociatedIrp.SystemBuffer, sizeof(SENDCMDINPARAMS) - 1);

		irp2 = IoBuildDeviceIoControlRequest(IOCTL_SCSI_MINIPORT,
			deviceExtension->PortDeviceObject,
			srbControl,
			sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS) - 1,
			srbControl,
			sizeof(SRB_IO_CONTROL) + length,
			FALSE,
			&event,
			&ioStatus);

		if (irp2 == NULL) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		// Call the port driver with the request and wait for it to complete.
		//

		status = IoCallDriver(deviceExtension->PortDeviceObject, irp2);

		if (status == STATUS_PENDING) {
			KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
			status = ioStatus.Status;
		}

		//
		// If successful, copy the data received into the output buffer
		//

		buffer = (ULONG_PTR)srbControl + srbControl->HeaderLength;

		if (NT_SUCCESS(status)) {

			RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer, (PVOID)buffer, length - 1);
			Irp->IoStatus.Information = length - 1;

			info = (IDINFO*)((SENDCMDOUTPARAMS*)Irp->AssociatedIrp.SystemBuffer)->bBuffer;
			DPRINT("[br]catch\t [%s]\n", info->sSerialNumber);
			snInfo.Count = 0;



			char tempname[128] = { 0 };
			GenRandHDSerial(&tempname);
			memcpy(info->sSerialNumber, tempname, strlen(tempname));

			GetSNInfo(&snInfo);
			for (i = 0; i < snInfo.Count; ++i)
			{
				GenRandHDSerial(&tempname);
				if (strlen(tempname) > 0)
				{
					memcpy(info->sSerialNumber, tempname, strlen(tempname));
					break;
				}
			}

			/*
			GetSNInfo(&snInfo);
			for (i = 0; i < snInfo.Count; ++i)
			{
				if (memcmp(info->sSerialNumber, snInfo.SNS[i].DiskSerial, 20) == 0) {
					memcpy(info->sSerialNumber, snInfo.SNS[i].ChangeTo, 20);
					DPRINT("[br]changes to\t [%s]\n", info->sSerialNumber);
					break;
				}
			}
			*/
		}
		else {

			RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer, (PVOID)buffer, (sizeof(SENDCMDOUTPARAMS) - 1));
			Irp->IoStatus.Information = sizeof(SENDCMDOUTPARAMS) - 1;

		}

		ExFreePool(srbControl);
	} while (0);

	Irp->IoStatus.Status = status;

	if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

		IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return(status);


	// if (NT_SUCCESS(status) && ctrlCode == SMART_RCV_DRIVE_DATA) {
	//     IDINFO* info = (IDINFO*)((SENDCMDOUTPARAMS*)Irp->AssociatedIrp.SystemBuffer)->bBuffer;
	//     KdPrint(("[br]catch\t [%s]\n", info->sSerialNumber));
	//     Update();
	//     for (; i < MAX_HD_COUNT; ++i)
	//     {
	//         if (memcmp(info->sSerialNumber, SNS[i].DiskSerial, 20) == 0) {
	//             for (j = 0; j < 20; ++j)
	//             {
	//                 info->sSerialNumber[j] = 0x33;
	//             }
	//             //memcpy(info->sSerialNumber, SNS[i].ChangeTo, 20);
	//             KdPrint(("[br]changes to\t [%s]\n", info->sSerialNumber));
	//             break;
	//         }
	//     }
	// }
	// return status;
}

BOOLEAN HookDiskDriver()
{
	UNICODE_STRING driverName;
	BOOLEAN ret = FALSE;
	NTSTATUS status;

	RtlInitUnicodeString(&driverName, VMProtectDecryptStringW(L"\\driver\\disk"));

	status = ObReferenceObjectByName(
		&driverName,
		OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		(PVOID *)&DiskDriver);

	do
	{
		if (DiskDriver == NULL) {
			DPRINT("[br] in [%s] ObReferenceObjectByName failure\n", __FUNCTION__);
			break;
		}

		RealDiskDeviceControl = DiskDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL];
		InterlockedExchangePointer((volatile PVOID *)&DiskDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL], HookedDiskDeviceControl);
		ret = TRUE;
	} while (0);

	SpooferRunning = 1;

	return ret;
}

BOOLEAN GetDiskSN(PDEVICE_OBJECT deviceObject, UCHAR* sn)
{
	NTSTATUS                status;
	PSENDCMDINPARAMS        pSCIP;
	PSENDCMDOUTPARAMS       pSCOP;
	PIRP                    Irp;
	IO_STATUS_BLOCK         ioStatus;
	KEVENT                  event;
	BOOLEAN                 ret = FALSE;

	pSCIP = (PSENDCMDINPARAMS)ExAllocatePool(NonPagedPool, sizeof(SENDCMDINPARAMS) - 1);
	pSCOP = (PSENDCMDOUTPARAMS)ExAllocatePool(NonPagedPool, sizeof(SENDCMDOUTPARAMS) + sizeof(IDINFO) - 1);

	if (pSCIP && pSCOP)
	{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		RtlZeroMemory(pSCIP, sizeof(SENDCMDINPARAMS) - 1);
		RtlZeroMemory(pSCOP, sizeof(SENDCMDOUTPARAMS) + sizeof(IDINFO) - 1);

		pSCIP->irDriveRegs.bCommandReg = IDE_ATA_IDENTIFY;
		pSCIP->cBufferSize = 0;
		pSCOP->cBufferSize = sizeof(IDINFO);

		if (Irp = IoBuildDeviceIoControlRequest(DFP_RECEIVE_DRIVE_DATA/*IRP_MJ_DEVICE_CONTROL*/, deviceObject, pSCIP, sizeof(SENDCMDINPARAMS) - 1,
			pSCOP, sizeof(SENDCMDOUTPARAMS) + sizeof(IDINFO) - 1, FALSE, &event, &ioStatus)) {
			status = IoCallDriver(deviceObject, Irp);
			if (status == STATUS_PENDING) {
				KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
				status = ioStatus.Status;
			}
			if (NT_SUCCESS(status)) {
				PIDINFO pinfo = (PIDINFO)pSCOP->bBuffer;
				memcpy(sn, pinfo->sSerialNumber, sizeof(pinfo->sSerialNumber));
				ret = TRUE;
			}
			else {
				DPRINT("[br] in [%s] IoCallDriver Failed: 0x%08X\n", __FUNCTION__, status);
			}
		}
	}
	if (pSCOP)
		ExFreePool(pSCOP);
	if (pSCIP)
		ExFreePool(pSCIP);

	return ret;
}


BOOLEAN SetOriginSNInfo()
{
	BOOLEAN ret = FALSE;
	NTSTATUS status;
	UNICODE_STRING driverName;
	PDEVICE_OBJECT deviceObject;
	PDRIVER_OBJECT driverObject;
	SNInfo sns;
	ULONG index = 0;
	OBJECT_ATTRIBUTES attr;
	ULONG result;
	HANDLE hReg = NULL;
	PKEY_VALUE_PARTIAL_INFORMATION pvpi = NULL;
	UNICODE_STRING valueStr;
	WCHAR valueBuf[4];
	WCHAR tmpBuf[82];
	ULONG ulSize;
	PWCHAR wptr;
	ULONG i = 0, j = 0;
	SNInfo oldInfo;
	BOOLEAN found = FALSE;


	sns.Count = 0;
	oldInfo.Count = 0;
	RtlInitUnicodeString(&driverName, VMProtectDecryptStringW(L"\\driver\\disk"));

	do
	{
		status = ObReferenceObjectByName(
			&driverName,
			OBJ_CASE_INSENSITIVE,
			NULL,
			0,
			*IoDriverObjectType,
			KernelMode,
			NULL,
			(PVOID *)&driverObject);
		if (driverObject == NULL) {
			DPRINT("[br] in [%s] ObReferenceObjectByName failure\n", __FUNCTION__);
			break;
		}

		deviceObject = driverObject->DeviceObject;
		while (deviceObject) {
			if (GetDiskSN(deviceObject, sns.SNS[index].DiskSerial)) {
				if (GetIndex(&sns, sns.SNS[index].DiskSerial) == -1) {
					index++;
					sns.Count = index;
				}
			}
			deviceObject = deviceObject->NextDevice;
		}

		ObDereferenceObject(driverObject);

		GetSNInfo(&oldInfo);

		for (i = 0; i < index; ++i)
		{
			found = FALSE;
			for (j = 0; j < oldInfo.Count; ++j)
			{
				if (memcmp(oldInfo.SNS[j].DiskSerial, sns.SNS[i].DiskSerial, 20) == 0) {
					memcpy(sns.SNS[i].ChangeTo, oldInfo.SNS[j].ChangeTo, 20);
					found = TRUE;
					break;
				}
			}
			if (!found) {
				memcpy(sns.SNS[i].ChangeTo, sns.SNS[i].DiskSerial, 20);
			}
		}

		InitializeObjectAttributes(&attr, &DriverRegistryPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
		status = ZwOpenKey(&hReg, KEY_WRITE, &attr);
		if (!NT_SUCCESS(status)) {
			DPRINT("[br]Failed to open registry [%ws] with write permission\n", DriverRegistryPath.Buffer);
			break;
		}

		valueBuf[0] = 's';
		valueBuf[1] = 'n';
		valueBuf[2] = '0';
		valueBuf[3] = '\0';
		RtlInitUnicodeString(&valueStr, valueBuf);

		for (i = 0; i < index; ++i)
		{
			wptr = tmpBuf;
			ToHexStr(sns.SNS[i].DiskSerial, SN_LEN, wptr);
			wptr += (SN_LEN * 2);
			*wptr++ = '|';
			ToHexStr(sns.SNS[i].ChangeTo, SN_LEN, wptr);
			wptr += (SN_LEN * 2);
			*wptr = 0;

			valueBuf[2] = '0' + (WCHAR)i;
			status = ZwSetValueKey(hReg, &valueStr, 0, REG_SZ, tmpBuf, (SN_LEN * 4 + 2) * sizeof(WCHAR));
			if (!NT_SUCCESS(status)) {
				DPRINT("[br]Writing the registry[%ws] failed\n", valueStr.Buffer);
				continue;
			}
		}
		ZwClose(hReg);
		ret = TRUE;
	} while (0);

	return ret;
}

VOID UnhookDiskDriver()
{
	InterlockedExchangePointer((volatile PVOID *)&DiskDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL], RealDiskDeviceControl);
	ObDereferenceObject(DiskDriver);
	DiskDriver = NULL;
	SpooferRunning = 0;
}


NTSTATUS DispatchRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}



/**
* Queries the name of an object.
*
* \param Object A pointer to an object.
* \param Buffer The buffer in which the object name will be stored.
* \param BufferLength The number of bytes available in \a Buffer.
* \param ReturnLength A variable which receives the number of bytes
* required to be available in \a Buffer.
*/
NTSTATUS KphQueryNameObject(
	__in PVOID Object,
	__out_bcount(BufferLength) POBJECT_NAME_INFORMATION Buffer,
	__in ULONG BufferLength,
	__out PULONG ReturnLength
)
{
	NTSTATUS status;
	POBJECT_TYPE objectType;

	PAGED_CODE();

	//objectType = KphGetObjectType(Object);

	// Check if we are going to hang when querying the object, and use
	// the special file object query function if needed.

	status = ObQueryNameString(Object, Buffer, BufferLength, ReturnLength);
	DPRINT("ObQueryNameString: status 0x%x\n", status);

	// Make the error returns consistent.
	if (status == STATUS_BUFFER_OVERFLOW) // returned by I/O subsystem
		status = STATUS_BUFFER_TOO_SMALL;
	if (status == STATUS_INFO_LENGTH_MISMATCH) // returned by ObQueryNameString
		status = STATUS_BUFFER_TOO_SMALL;

	if (NT_SUCCESS(status))
		DPRINT("KphQueryNameObject: %.*S\n", Buffer->Name.Length / sizeof(WCHAR), Buffer->Name.Buffer);
	else
		DPRINT("KphQueryNameObject: status 0x%x\n", status);

	return status;
}

void GenRandHDSerial(char *oBuf)
{
	char serialbuff[256];
	char unibuff[256] = { 0 };
	char randbuff[256] = { 0 };
	RandString((char *)&randbuff, 14);
	sprintf(oBuf, VMProtectDecryptStringA("S%d%s"), RandRange(1, 3), randbuff);
}