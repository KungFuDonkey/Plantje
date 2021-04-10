#include "Arduino.h"
class Encryptor
{
public:
	char* Decrypt(const char* msg, const char* topic, int topicSize)
	{
		int key = 0;

		
		char* encryption = TrimKey(msg, key);
		char* topicWithKey = AppendKey(topic, key, topicSize);
		
		
		int topicInterpolated[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateTopicHash(topicWithKey,topicInterpolated, topicSize);
		char normalEncryption[18] = {
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*',
			'*','*'
		};
		CreateEncryption(topicInterpolated,normalEncryption);
		
		char trimmedEncryption[16] = {
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*'
		};
		TrimEncryption(normalEncryption,trimmedEncryption);
		
		int normalEncryptionArray[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateBounds(trimmedEncryption,16,normalEncryptionArray);
		int encryptionArray[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateBounds(encryption, 16, encryptionArray);



		int length = 0;
		for (int i = 0; i < 16; i++) {
			if (encryptionArray[i] == normalEncryptionArray[i]) {
				break;
			}
			length++;
		}

		delete[] decrypt;
		decrypt = new char[length + 1];
		
		for (int i = 0; i < length; i++)
		{
			int interpolatedAnswer = encryptionArray[i] - normalEncryptionArray[i];
			if (interpolatedAnswer < 0)
			{
				interpolatedAnswer += 67;
			}
			if (interpolatedAnswer < 44)
			{
				decrypt[i] = (char)(interpolatedAnswer + 47);
				continue;
			}
			decrypt[i] = (char)(interpolatedAnswer + 54);
		}
		decrypt[length] = '\0';
		return decrypt;
	}

	char* Encrypt(const char* msg, int size, const char* topic, int topicSize)
	{
		int key = random(100);
		char* topicWithKey = AppendKey(topic, key, topicSize);
		
		int topicInterpolated[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateTopicHash(topicWithKey,topicInterpolated, topicSize);
		char normalEncryption[18] = {
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*'
		};
		CreateEncryption(topicInterpolated,normalEncryption);

		delete[] msgArray;
		int* msgArray = new int[size];
		CreateBounds(msg, size, msgArray);

		for (int i = 0; i < size; i++)
		{
			topicInterpolated[i] += msgArray[i];
			if (topicInterpolated[i] > 67)
			{
				topicInterpolated[i] -= 67;
			}
		}
		delete[] encrypt;
		encrypt = new char[19];
		encrypt[18] = '\0';
		CreateEncryption(topicInterpolated, encrypt);
		AddKeyToEncryption(key, encrypt);

		return encrypt;
	}
private:
	char* decrypt = new char[1]{ '\0' };
	char* encrypt = new char[1]{ '\0' };
	int* msgArray = new int[0]{};


	char* newArray = new char[1]{ '\0' };
	char* AppendKey(const char* topic, int key, int topicSize) {
		delete[] newArray;
		newArray = new char[topicSize + 3];
		for (int i = 0; i < topicSize; i++) {
			newArray[i] = topic[i];
		}
		newArray[topicSize] = (char)(key / 10 + 48);
		newArray[topicSize + 1] = (char)(key % 10 + 48);
		newArray[topicSize + 2] = '\0';
		return newArray;
	}

	void TrimEncryption(char array[], char outputarray[]) {
		for (int i = 0; i < 16; i++) {
			outputarray[i] = array[i + 1];
		}
	}

	char* newmessage = new char[1]{ '\0' };
	char* TrimKey(const char* msg, int& key) {
		char keyArray[2];
		int firstInt = 0;
		int sndInt = 0;
		for (int i = 0; i < 17; i++)
		{
			if (msg[i] < 58)
			{
				keyArray[0] = msg[i];
				firstInt = i;
				break;
			}
		}
		for (int i = 17; i >= 0; i--)
		{
			if (msg[i] < 58)
			{
				keyArray[1] = msg[i];
				sndInt = i;
				break;
			}
		}
		delete[] newmessage;
		newmessage = new char[16];
		int numerator = 0;
		for (int i = 0; i < 18; i++) {
			if (i != firstInt && i != sndInt) {
				newmessage[numerator] = msg[i];
				numerator++;
			}
		}
		key = atoi(keyArray);
		return newmessage;
	}

	void CreateEncryption(int topicInterpolated[], char encryption[])
	{
		for (int i = 0; i < 16; i++)
		{
			if (topicInterpolated[i] < 44)
			{
				encryption[i + 1] = (char)(topicInterpolated[i] + 47);
				continue;
			}
			encryption[i + 1] = (char)(topicInterpolated[i] + 54);
		}
	}

	void AddKeyToEncryption(int key, char* encryption)
	{
		char character1 = (char)(key / 10 + 48);
		char character2 = (char)(key % 10 + 48);
		for (int i = 1; i < 16; i++)
		{
			if (encryption[i] < 58 || random(100) < 30)
			{
				for (int x = 17; x > i; x--)
				{
					encryption[x] = encryption[x - 1];
				}
				encryption[i] = character1;
				break;
			}
		}

		for (int i = 17; i >= 1; i--)
		{
			if (encryption[i] < 58 || random(100) < 30)
			{
				for (int x = 0; x < i; x++)
				{
					encryption[x] = encryption[x + 1];
				}
				encryption[i] = character2;
				break;
			}
		}
	}

	void CreateTopicHash(char topicWithKey[], int topicInterpolated[], int topicSize)
	{
		int* topicWithKeyArray = new int[topicSize + 2];
		CreateBounds(topicWithKey, topicSize + 2, topicWithKeyArray);
		for (int i = 0; i < topicSize + 2; i++)
		{
			topicInterpolated[i % 16] = topicInterpolated[i % 16] + topicWithKeyArray[i];
			if (topicInterpolated[i % 16] > 67)
			{
				topicInterpolated[i % 16] -= 67;
			}
		}
		delete[] topicWithKeyArray;
		ScanR(topicInterpolated);
		ScanL(topicInterpolated);
	}

	void CreateBounds(const char* msg, int size, int array[])
	{
		for (int i = 0; i < size; i++)
		{
			if (msg[i] < 91)
			{
				array[i] = msg[i] - 47;
				continue;
			}
			array[i] = msg[i] - 54;
		}
	}

	void ScanL(int topicInterpolated[])
	{
		for (int i = 14; i >= 0; i--)
		{
			topicInterpolated[i] += topicInterpolated[i + 1];
			if (topicInterpolated[i] > 67)
			{
				topicInterpolated[i % 16] -= 67;
			}
		}
	}

	void ScanR(int topicInterpolated[])
	{
		for (int i = 1; i < 16; i++)
		{
			topicInterpolated[i] += topicInterpolated[i - 1];
			if (topicInterpolated[i] > 67)
			{
				topicInterpolated[i % 16] -= 67;
			}
		}
	}
};