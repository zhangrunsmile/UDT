#include <string.h>
	pthread_cond_signal(&Send_Cond);
	pthread_join(_hSendThread, NULL);
	pthread_mutex_destroy(&Send_Lock);
	pthread_cond_destroy(&Send_Cond);
#else
	SetEvent(Send_Cond);
	WaitForSingleObject(_hSendThread, INFINITE);
	CloseHandle(_hSendThread);
	CloseHandle(Send_Lock);
	CloseHandle(Send_Cond);
#endif
	jobject thiz;

	memset(context, 0, sizeof(ServerContext));
	context->env = env;
	env->GetJavaVM(&(context->jvm));
	context->thiz = (context->env)->NewGlobalRef(thiz);

	context->pstrAddr = new char[strlen(pstrAddr)];
	strcpy(context->pstrAddr, pstrAddr);

	context->pstrHostName = new char[strlen(pstrHostName)];
	strcpy(context->pstrHostName, pstrHostName);

	context->pstrSendtype = new char[strlen(pstrSendtype)];
	strcpy(context->pstrSendtype, pstrSendtype);
	context->strArray = strArray;
	context->nPort = nPort;
	context->eType = eType;

#ifndef WIN32
	pthread_mutex_init(&Send_Lock, NULL);
	pthread_cond_init(&Send_Cond, NULL);
	pthread_create(&_hSendThread, NULL, _SendThreadProc, context);
	pthread_detach(_hSendThread);
#else
	DWORD ThreadID;
	Send_Lock = CreateMutex(NULL, false, NULL);
	Send_Cond = CreateEvent(NULL, false, false, NULL);
	_hSendThread = CreateThread(NULL, 0, _SendThreadProc, context, NULL, &ThreadID);
#endif
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	char szPort[64];
	memset(szPort, 0, 64);
	sprintf(szPort,"%d", cxt->nPort);
	if (0 != getaddrinfo(cxt->pstrAddr, szPort, &hints, &peer))
	{
		szError = "getaddrinfo fail:";
		goto Loop;
	}

	fhandle = UDT::socket(peer->ai_family, peer->ai_socktype, peer->ai_protocol);

	// connect to the server, implict bind
	if (UDT::ERROR == UDT::connect(fhandle, peer->ai_addr, peer->ai_addrlen))
	{
		goto Loop;
	}
	freeaddrinfo(peer);
			// ��Ϣ����֪ͨ��MSR��
		//	log << "Send message" << endl;

			char head[8];
			memset(head, 0, 8);
			strcpy(head, "TSR");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

			// send name information of the requested file
			string actualName = cxt->pstrHostName;
			int len = actualName.size();
			if (UDT::ERROR == UDT::send(fhandle, (char*)&len, sizeof(int), 0))
			{
				goto Loop;
			}
			if (UDT::ERROR == UDT::send(fhandle, actualName.c_str(), len, 0))
			{
				goto Loop;
			}

			// send message size and information
			jstring jszMsg = (jstring)(cxt->env)->GetObjectArrayElement(cxt->strArray, 0);
			string strMessage = (char*)(cxt->env)->GetStringUTFChars(jszMsg, &b);
			int nLen = strMessage.size();
			if (UDT::ERROR == UDT::send(fhandle, (char*)&nLen, sizeof(int), 0))
			{
				goto Loop;
			}
			if (UDT::ERROR == UDT::send(fhandle, strMessage.c_str(), nLen, 0))
			{
				goto Loop;
			}

		//	log << "send message fineshed " << endl;
			// ���ļ���������FSR��
		//	log << "Send unifile" << endl;

			char head[8];
			memset(head, 0, 8);
			strcpy(head, "FSR");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

			jstring jszFileName = (jstring)(cxt->env)->GetObjectArrayElement(cxt->strArray, 0);
			string strFileName = (char*)(cxt->env)->GetStringUTFChars(jszFileName, &b);

			string actualName;
			int pos = strFileName.find_last_of('/');
			actualName = strFileName.substr(pos+1);

			actualName += "\\";
			actualName += cxt->pstrHostName;
			if (0 == strcmp("GENIETURBO", cxt->pstrSendtype))
			{
				actualName += "\\";
				actualName += cxt->pstrSendtype;
			}

			// send name information of the requested file
			int nLen = actualName.size();
			if (UDT::ERROR == UDT::send(fhandle, (char*)&nLen, sizeof(int), 0))
			{
				goto Loop;
			}
			if (UDT::ERROR == UDT::send(fhandle, actualName.c_str(), nLen, 0))
			{
				goto Loop;
			}


			//�ļ�����Ӧ��FSP��
			memset(head, 0, 8);
			if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
			{
				goto Loop;
			}

		//	log << "Recv FSP:" << head <<endl;
			if (strcmp(head, "FSP1") != 0)
			{
				szFinish = "";
				goto Loop;
			}

			//�ļ����ݴ��ͣ�FCS��
			memset(head, 0, 8);
			strcpy(head, "FCS");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

			// open the file
			fstream ifs(strFileName.c_str(), ios::in | ios::binary);
			ifs.seekg(0, ios::end);
			int64_t size = ifs.tellg();
			ifs.seekg(0, ios::beg);

			// send file size information
			if (UDT::ERROR == UDT::send(fhandle, (char*)&size, sizeof(int64_t), 0))
			{
				goto Loop;
			}

		//	log << "Send file name :" << actualName << ", file size:" << size << endl;
			// send the file
			int64_t offset = 0;
			int64_t left = size;
			int64_t iLastPercent = 0;
			jstring jsfile = (cxt->env)->NewStringUTF(actualName.c_str());
			while(left > 0)
			{
				int send = 0;
				if (left > 51200)
					send = UDT::sendfile(fhandle, ifs, offset, 51200);
				else
					send = UDT::sendfile(fhandle, ifs, offset, left);

				if (UDT::ERROR == send)
				{
					goto Loop;
				}
				left -= send;
				offset += send;

				// ����Ӧ�𡢱�֤��������յĽ���ͳһ
				int iPercent = (offset*100)/size;
				if (iPercent == 1)
				{
					iPercent = 1;
				}
				if (iPercent > 100)
				{
					iPercent = iPercent;
				}
				if (iPercent != iLastPercent)
				{
				//	log << "recv percent, has send size:" << offset << endl;
					iLastPercent = iPercent;
					//�ļ�����Ӧ��FCS��
					memset(head, 0, 8);
					if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
					{
						goto Loop;
					}
				}

				if (send > 0)
				{
					(cxt->env)->CallVoidMethod(cxt->thiz, mOnTransfer, size, offset, jsfile);
				}
			}
			ifs.close();
			// ���ļ���������MSR��
		//	log << "Send multifile" << endl;

			char head[8];
			memset(head, 0, 8);
			strcpy(head, "MSR");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

			// send total size
			jstring jsFileName;
			string oneFileName, strFileName;
			int64_t oneFileNameSize;
			int64_t totalSize = 0;
			int totalCount = 0;
			int row = (cxt->env)->GetArrayLength(cxt->strArray);//�������
			for(int i = 0; i < row; i++)
			{
				jsFileName = (jstring)(cxt->env)->GetObjectArrayElement(cxt->strArray, i);
				strFileName = (char*)(cxt->env)->GetStringUTFChars(jsFileName, &b);

				fstream ifs(strFileName.c_str(), ios::in | ios::binary);
				ifs.seekg(0, ios::end);
				int64_t size = ifs.tellg();
				totalSize += size;
				totalCount++;
				ifs.seekg(0, ios::beg);
				if (i == 0)
				{
					oneFileName = strFileName;
					oneFileNameSize = strFileName.length();
				}
				ifs.close();
			}

		//	log << "MultiFiles total size:" << totalSize << "totalCount:" << totalCount << endl;
			if (UDT::ERROR == UDT::send(fhandle, (char *)&totalSize, sizeof(int64_t), 0))
			{
				goto Loop;
			}

			// send file count
			if (UDT::ERROR == UDT::send(fhandle, (char*)&totalCount, sizeof(int), 0))
			{
				goto Loop;
			}

			string tmp = oneFileName;
			string actualName;
			int pos = oneFileName.find_last_of('/');
			actualName = tmp.substr(pos+1);
			actualName += "\\";
			actualName += cxt->pstrHostName;
			if (0 == strcmp("GENIETURBO", cxt->pstrSendtype))
			{
				actualName += "\\";
				actualName += cxt->pstrSendtype;
			}

			int actualLength = actualName.length();

			// send file name size
			if (UDT::ERROR == UDT::send(fhandle, (char*)&actualLength, sizeof(int), 0))
			{
				goto Loop;
			}
			if (UDT::ERROR == UDT::send(fhandle, actualName.c_str(), actualLength, 0))
			{
				goto Loop;
			}

			//�ļ�����Ӧ��MSP��
			memset(head, 0, 8);
			if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
			{
				goto Loop;
			}

		//	log << "Recv request MSP:" << head << endl;
			if (strcmp(head, "MSP1") != 0)
			{
				szFinish = "";
				goto Loop;
			}

			int64_t sendsize = 0;
			int64_t iLastPercent = 0;
			for(int j = 0; j < row; j++)
			{
				memset(head, 0, 8);
				strcpy(head, "MCS");
				if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
				{
					goto Loop;
				}

				jsFileName = (jstring)(cxt->env)->GetObjectArrayElement(cxt->strArray, j);
				strFileName = (char*)(cxt->env)->GetStringUTFChars(jsFileName, &b);

				string tmp = strFileName;
				string actualName;
				int pos = strFileName.find_last_of('/');
				actualName = tmp.substr(pos+1);
				int actualLength = actualName.length();

				if (UDT::ERROR == UDT::send(fhandle, (char*)&actualLength, sizeof(int), 0))
				{
					goto Loop;
				}
				if (UDT::ERROR == UDT::send(fhandle, actualName.c_str(), actualLength, 0))
				{
					goto Loop;
				}
				jstring jsname = (cxt->env)->NewStringUTF(actualName.c_str());

				fstream ifs(strFileName.c_str(), ios::in | ios::binary);
				ifs.seekg(0, ios::end);
				int64_t size = ifs.tellg();
				ifs.seekg(0, ios::beg);

				// send file size information
				if (UDT::ERROR == UDT::send(fhandle, (char*)&size, sizeof(int64_t), 0))
				{
					goto Loop;
				}

			//	log << "send file full name and size:" << strFileName << "-" << size << endl;
				//UDT::TRACEINFO trace;
				//UDT::perfmon(fhandle, &trace);

				// send the file
				int64_t offset = 0;
				int64_t left = size;
				while(left > 0)
				{
					int send = 0;
					if (left > 51200)
						send = UDT::sendfile(fhandle, ifs, offset, 51200);
					else
						send = UDT::sendfile(fhandle, ifs, offset, left);

					if (UDT::ERROR == send)
					{
						goto Loop;
					}

					left -= send;
					offset += send;
					sendsize += send;

					// ����Ӧ�𡢱�֤��������յĽ���ͳһ
					int iPercent = (sendsize*100)/totalSize;
					if (iPercent == 1)
					{
						iPercent = 1;
					}
					if (iPercent > 100)
					{
						iPercent = iPercent;
					}
					if (iPercent != iLastPercent)
					{
					//	log << "recv percent, has send size:" << sendsize << endl;
						iLastPercent = iPercent;
						//�ļ�����Ӧ��FCS��
						memset(head, 0, 8);
						if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
						{
							goto Loop;
						}
					}
					(cxt->env)->CallVoidMethod(cxt->thiz, mOnTransfer, totalSize, sendsize, jsname);
				}
				ifs.close();

			//	log << "send file finished:" << strFileName << endl;

				//UDT::perfmon(fhandle, &trace);
				//cout << "speed = " << trace.mbpsSendRate << "Mbits/sec" << endl;
			}

			memset(head, 0, 8);
			strcpy(head, "MSF");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

		//	log << "Send multiFile finished" << endl;
			// �ļ��д�������DSR��
		//	log << "Send folderFile" << endl;

			char head[8];
			memset(head, 0, 8);
			strcpy(head, "DSR");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

			jstring jszFolderName = (jstring)(cxt->env)->GetObjectArrayElement(cxt->strArray, 0);
			string strFolderName = (char*)(cxt->env)->GetStringUTFChars(jszFolderName, &b);

			string tmp = strFolderName;
			string folderName, devName, actualName;
			string fileName, filePath;

			int64_t totalFileSize = 0;
			vector<string> vecFilePath;
			searchFileInDirectroy(strFolderName.c_str(), totalFileSize, vecFilePath);
		//	log << "Foler name and  totalSize:" << strFolderName << "-" << totalFileSize << endl;

			int pos = tmp.find_last_of('/');
			folderName = tmp.substr(pos+1);
			actualName = tmp.substr(pos+1);
			actualName += "\\";
			actualName += cxt->pstrHostName;
			if (0 == strcmp("GENIETURBO", cxt->pstrSendtype))
			{
				actualName += "\\";
				actualName += cxt->pstrSendtype;
			}

			// send file count size
			if (UDT::ERROR == UDT::send(fhandle, (char*)&totalFileSize, sizeof(int64_t), 0))
			{
				goto Loop;
			}

			int actualLength = actualName.length();
			// send foldername size
			if (UDT::ERROR == UDT::send(fhandle, (char *)&actualLength, sizeof(int), 0))
			{
				goto Loop;
			}
			// send foldername
			if (UDT::ERROR == UDT::send(fhandle, actualName.c_str(), actualLength, 0))
			{
				goto Loop;
			}

			//�ļ��д���Ӧ��FSP��
			memset(head, 0, 8);
			if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
			{
				goto Loop;
			}
			//if (memcmp(head, "DSP1", 0)!=0)
			if (strcmp(head, "DSP1") != 0)
			{
				szFinish = "";
				goto Loop;
			}

			string strTempForlder;
			vector<string> vecDir;
			//���ļ��д�������
			for(int i = 0; i < vecFilePath.size(); i++)
			{
				int len = 0;
				tmp = vecFilePath[i];
				int pos = tmp.find_last_of('\\');
				if (pos > 0)
				{
					strTempForlder = tmp.substr(0,pos);
					tmp = tmp.substr(0,pos);
				}
				else
				{
					pos = tmp.find_last_of('/');
					filePath = tmp.substr(0,pos);
					tmp = tmp.substr(0,pos);
				}

				len = strFolderName.size() - folderName.size();
				tmp = filePath.substr(len);
				for (int j = 0; j < vecDir.size(); j++)
				{
					if (vecDir[j].compare(tmp) == 0)
					{
						break;
					}
				}
				vecDir.push_back(tmp);

				memset(head, 0, 8);
				strcpy(head,"DCR");
				if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
				{
					goto Loop;
				}

				len = tmp.size();
				if (UDT::ERROR == UDT::send(fhandle, (char*)&len, sizeof(int), 0))
				{
					goto Loop;
				}

				if (UDT::ERROR == UDT::send(fhandle, tmp.c_str(), len, 0))
				{
					goto Loop;
				}

			//	log << "Foldername :" << tmp << endl;
			}

			int64_t sendsize = 0;
			int64_t iLastPercent = 0;
			//Ŀ¼�ļ���������
			for(int i = 0; i < vecFilePath.size(); i++)
			{
				memset(head, 0, 8);
				strcpy(head,"DFS");
				if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
				{
					goto Loop;
				}

				int len = 0;
				filePath = vecFilePath[i];

				len = strFolderName.size() - folderName.size();
				fileName = filePath.substr(len);
			//	log << "file name :" << fileName << "\n" << endl;

				len = fileName.size();
				if (UDT::ERROR == UDT::send(fhandle, (char*)&len, sizeof(int), 0))
				{
					goto Loop;
				}

				if (UDT::ERROR == UDT::send(fhandle, fileName.c_str(), len, 0))
				{
					goto Loop;
				}

				string actualName;
				int pos = fileName.find_last_of('/');
				actualName = fileName.substr(pos+1);
				jstring jsname = (cxt->env)->NewStringUTF(actualName.c_str());

				// open the file
				fstream ifs(filePath.c_str(), ios::in | ios::binary);
				ifs.seekg(0, ios::end);
				int64_t size = ifs.tellg();
				ifs.seekg(0, ios::beg);

				// send file size information
				if (UDT::ERROR == UDT::send(fhandle, (char*)&size, sizeof(int64_t), 0))
				{
					goto Loop;
				}

			//	log << "send file full name and size :" << filePath << "-" << size << endl;
				// send the file
				int64_t offset = 0;
				int64_t left = size;
				while(left > 0)
				{
					int send = 0;
					if (left > 51200)
						send = UDT::sendfile(fhandle, ifs, offset, 51200);
					else
						send = UDT::sendfile(fhandle, ifs, offset, left);

					if (UDT::ERROR == send)
					{
						goto Loop;
					}
					left -= send;
					offset += send;
					sendsize += send;

					// ����Ӧ�𡢱�֤��������յĽ���ͳһ
					int iPercent = (sendsize*100)/totalFileSize;
					if (iPercent == 1)
					{
						iPercent = 1;
					}
					if (iPercent > 100)
					{
						iPercent = iPercent;
					}
					if (iPercent != iLastPercent)
					{
					//	log << "recv percent, has send size:" << sendsize << endl;
						iLastPercent = iPercent;
						//�ļ�����Ӧ��FCS��
						memset(head, 0, 8);
						if (UDT::ERROR == UDT::recv(fhandle, (char*)head, 4, 0))
						{
							goto Loop;
						}
					}
					(cxt->env)->CallVoidMethod(cxt->thiz, mOnTransfer, totalFileSize, sendsize, jsname);
				}
				ifs.close();

			//	log << "send file finished:" << filePath << endl;
			}

			//Ŀ¼�������
			memset(head, 0, 8);
			strcpy(head, "DSF");
			if (UDT::ERROR == UDT::send(fhandle, (char*)head, 3, 0))
			{
				goto Loop;
			}

		//	log << "Send finished\n" << endl;
	//	log << "SendType error" << endl;
		break;
		jsOutput = (cxt->env)->NewStringUTF(szFinish.c_str());
		(cxt->env)->CallVoidMethod(cxt->thiz, mOnFinished, jsOutput);

	// Debug info
	if (szError.empty())
		(cxt->env)->CallVoidMethod(cxt->thiz, mOnDebug, jsOutput);

	UDT::close(fhandle);
			{
				strTemp = fn.substr(strFolder.size());
				int nPos = strTemp.find_first_of('/');
				if (nPos > 0)
				{
					strFolder = strFolder + strTemp.substr(0, nPos);
					if ((pDir = opendir(strFolder.c_str())) == NULL)
					strFolder += "/";
				}
			}

		(env)->SetObjectArrayElement(arry, 0, message);

		(env)->SetObjectArrayElement(arry, 0, fileName);

		(env)->SetObjectArrayElement(arry, 0, folderName);