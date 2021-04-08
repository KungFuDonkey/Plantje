#include "Arduino.h"
class Encryptor
{
public:
	String Decrypt(String msg, String topic)
	{
		int key = 0;
		String encryption = TrimKey(msg, key);
		String topicWithKey = topic + String(key);
		
		
		int topicInterpolated[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateTopicHash(topicWithKey,topicInterpolated);
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
		String thing = ToString(trimmedEncryption, 16);
		CreateBounds(thing,normalEncryptionArray);
		int encryptionArray[16] = {
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0
		};
		CreateBounds(encryption,encryptionArray);

		char answer[16] = {
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*',
			'*','*','*','*'
		};

		for (int i = 0; i < 16; i++)
		{
			int interpolatedAnswer = encryptionArray[i] - normalEncryptionArray[i];
			if (interpolatedAnswer < 0)
			{
				interpolatedAnswer += 67;
			}
			if (interpolatedAnswer == 0)
			{
				answer[i] = ' ';
				continue;
			}
			if (interpolatedAnswer < 44)
			{
				answer[i] = (char)(interpolatedAnswer + 47);
				continue;
			}
			answer[i] = (char)(interpolatedAnswer + 54);
		}

		String message = ToString(answer, 16);
		return message;
	}

	String Encrypt(String msg, String topic)
	{
    int key = rand() % 100;
		String keyString = String(key);
		String topicWithKey = topic + keyString;
		
		int topicInterpolated[16];
		CreateTopicHash(topicWithKey,topicInterpolated);
		char normalEncryption[16];
		CreateEncryption(topicInterpolated,normalEncryption);

		int* msgArray = new int[msg.length()];
		CreateBounds(msg,msgArray);

		for (int i = 0; i < msg.length(); i++)
		{
			topicInterpolated[i] += msgArray[i];
			if (topicInterpolated[i] > 67)
			{
				topicInterpolated[i] -= 67;
			}
		}
		char encryption[16];
		CreateEncryption(topicInterpolated,encryption);
		AddKeyToEncryption(keyString, encryption);

		return String(encryption);
	}
private:

	void TrimEncryption(char array[], char outputarray[]) {
		for (int i = 0; i < 16; i++) {
			outputarray[i] = array[i + 1];
		}
	}

	String TrimKey(String msg, int& key) {
		char keyArray[2];
		int firstInt = 0;
		int sndInt = 0;
		for (int i = 0; i < msg.length(); i++)
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
		String newmessage = "                ";
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

	void AddKeyToEncryption(String keyString, char* encryption)
	{
		for (int i = 1; i < 16; i++)
		{
			if (encryption[i] < 58 || (rand() % 100) < 30)
			{
				for (int x = 17; x > i; x--)
				{
					encryption[x] = encryption[x - 1];
				}
				encryption[i] = keyString[0];
				break;
			}
		}

		for (int i = 17; i >= 1; i--)
		{
			if (encryption[i] < 58 || (rand() % 100) < 30)
			{
				for (int x = 0; x < i; x++)
				{
					encryption[x] = encryption[x + 1];
				}
				encryption[i] = keyString[1];
				break;
			}
		}
	}

	void CreateTopicHash(String topicWithKey, int topicInterpolated[])
	{
		int topicWithKeyArray[16];
		CreateBounds(topicWithKey, topicWithKeyArray);
		for (int i = 0; i < topicWithKey.length(); i++)
		{
			topicInterpolated[i % 16] = topicInterpolated[i % 16] + topicWithKeyArray[i];
			if (topicInterpolated[i % 16] > 67)
			{
				topicInterpolated[i % 16] -= 67;
			}
		}
		ScanR(topicInterpolated);
		ScanL(topicInterpolated);
	}

	void CreateBounds(String msg, int array[])
	{
		for (int i = 0; i < msg.length(); i++)
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

	String ToString(char* charArray, int size) {
		String s;
		for (int i = 0; i < size; i++) {
			s += charArray[i];
		}
		return s;
	}
};