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

static const char *SkipWhitespace(const char *Config)
{
	while (*Config == ' ' || *Config == '\t' || IsLineBreak(*Config)) {
		++Config;
	}

	return Config;
}

static const char *SkipCommentOrUnknownLine(const char *Config)
{
	while (*Config != '\0' && !IsLineBreak(*Config)) {
		++Config;
	}

	return Config;
}

static bool IsValueTerminator(char Character)
{
	return Character == '\0' || Character == ' ' || Character == '\t' ||
		   IsLineBreak(Character);
}

static bool ConfigValueEquals(const char *Value, const char *Expected)
{
	while (*Expected != '\0') {
		char Character = *Value++;

		if (Character >= 'A' && Character <= 'Z') {
			Character = (char)(Character - 'A' + 'a');
		}

		if (Character != *Expected++) {
			return false;
		}
	}

	return IsValueTerminator(*Value);
}

bool ParseKernelPath(const char *Config, char KernelPath[BootPathMax])
{
	while (*Config != '\0') {
		Config = SkipWhitespace(Config);

		if (*Config == '#') {
			Config = SkipCommentOrUnknownLine(Config);
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

		Config = SkipCommentOrUnknownLine(Config);
	}

	return false;
}

bool ParseFramebufferEnabled(const char *Config)
{
	while (*Config != '\0') {
		Config = SkipWhitespace(Config);

		if (*Config == '#') {
			Config = SkipCommentOrUnknownLine(Config);
			continue;
		}

		if (StringStartsWith(Config, FramebufferConfigKey)) {
			const char *Value = Config + sizeof(FramebufferConfigKey) - 1u;

			return !(ConfigValueEquals(Value, "no") ||
					 ConfigValueEquals(Value, "false") ||
					 ConfigValueEquals(Value, "off") ||
					 ConfigValueEquals(Value, "0"));
		}

		Config = SkipCommentOrUnknownLine(Config);
	}

	return true;
}

bool ParseCmdline(const char *Config, char Cmdline[BootCmdlineMax])
{
	while (*Config != '\0') {
		Config = SkipWhitespace(Config);

		if (*Config == '#') {
			Config = SkipCommentOrUnknownLine(Config);
			continue;
		}

		if (StringStartsWith(Config, CmdlineConfigKey)) {
			Config += sizeof(CmdlineConfigKey) - 1u;
			size_t Length = 0;

			while (Config[Length] != '\0' && !IsLineBreak(Config[Length])) {
				if (Length + 1u >= BootCmdlineMax) {
					return false;
				}

				Cmdline[Length] = Config[Length];
				++Length;
			}

			while (Length > 0 && (Cmdline[Length - 1u] == ' ' ||
								  Cmdline[Length - 1u] == '\t')) {
				--Length;
			}

			Cmdline[Length] = '\0';
			return true;
		}

		Config = SkipCommentOrUnknownLine(Config);
	}

	Cmdline[0] = '\0';
	return true;
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
