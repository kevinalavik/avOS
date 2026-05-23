#include <Device/Disk.h>
#include <Filesystem/Vfs.h>
#include <Library/ConfigParser.h>
#include <Memory/Allocator.h>

static bool StringStartsWith(const char *String, const char *Prefix)
{
	while (*Prefix != '\0') {
		if (*String++ != *Prefix++) {
			return false;
		}
	}

	return true;
}

static bool IsLineBreak(char Character)
{
	return Character == '\r' || Character == '\n';
}

bool ParseKernelPath(const char *Config, char KernelPath[BootPathMax])
{
	while (*Config != '\0') {
		while (*Config == ' ' || *Config == '\t' || IsLineBreak(*Config)) {
			++Config;
		}

		if (*Config == '#') {
			while (*Config != '\0' && !IsLineBreak(*Config)) {
				++Config;
			}
			continue;
		}

		if (StringStartsWith(Config, KernelConfigKey)) {
			Config += sizeof(KernelConfigKey) - 1u;
			size_t PathLength = 0;

			while (Config[PathLength] != '\0' &&
				   !IsLineBreak(Config[PathLength])) {
				if (PathLength + 1u >= BootPathMax) {
					return false;
				}

				KernelPath[PathLength] = Config[PathLength];
				++PathLength;
			}

			while (PathLength > 0 && (KernelPath[PathLength - 1u] == ' ' ||
									  KernelPath[PathLength - 1u] == '\t')) {
				--PathLength;
			}

			KernelPath[PathLength] = '\0';
			return PathLength != 0;
		}

		while (*Config != '\0' && !IsLineBreak(*Config)) {
			++Config;
		}
	}

	return false;
}

bool ReadTextFile(const char *Path, char **BufferOut)
{
	File FileHandle;

	if (!FileOpen(Path, &FileHandle)) {
		return false;
	}

	size_t Size = FileSize(&FileHandle);
	char *Buffer = Alloc(Size + 1u, 16);
	if (Buffer == 0) {
		FileClose(&FileHandle);
		return false;
	}

	size_t Read = FileRead(&FileHandle, Buffer, Size);
	Buffer[Read] = '\0';
	FileClose(&FileHandle);

	*BufferOut = Buffer;
	return Read == Size;
}
