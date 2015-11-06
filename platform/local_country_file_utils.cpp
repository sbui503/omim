#include "platform/local_country_file_utils.hpp"

#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/logging.hpp"
#include "base/regexp.hpp"

#include "std/algorithm.hpp"
#include "std/cctype.hpp"
#include "std/sstream.hpp"
#include "std/unique_ptr.hpp"
#include "std/unordered_set.hpp"

#include "defines.hpp"

namespace platform
{
namespace
{
char const kBitsExt[] = ".bftsegbits";
char const kNodesExt[] = ".bftsegnodes";
char const kOffsetsExt[] = ".offsets";

size_t const kMaxTimestampLength = 18;

bool GetFileTypeChecked(string const & path, Platform::EFileType & type)
{
  Platform::EError const ret = Platform::GetFileType(path, type);
  if (ret != Platform::ERR_OK)
  {
    LOG(LERROR, ("Can't determine file type for", path, ":", ret));
    return false;
  }
  return true;
}

bool MkDirChecked(string const & directory)
{
  Platform & platform = GetPlatform();
  Platform::EError const ret = platform.MkDir(directory);
  switch (ret)
  {
    case Platform::ERR_OK:
      return true;
    case Platform::ERR_FILE_ALREADY_EXISTS:
    {
      Platform::EFileType type;
      if (!GetFileTypeChecked(directory, type))
        return false;
      if (type != Platform::FILE_TYPE_DIRECTORY)
      {
        LOG(LERROR, (directory, "exists, but not a directory:", type));
        return false;
      }
      return true;
    }
    default:
      LOG(LERROR, (directory, "can't be created:", ret));
      return false;
  }
}

string GetSpecialFilesSearchScope()
{
#if defined(OMIM_OS_ANDROID)
  return "er";
#else
  return "r";
#endif  // defined(OMIM_OS_ANDROID)
}

void DeleteDownloaderFilesForAllCountries(string const & directory)
{
  static string const regexp = "\\.(downloading|resume|ready)[0-9]?$";
  Platform::FilesList files;
  Platform::GetFilesByRegExp(directory, regexp, files);
  for (auto const & file : files)
    my::DeleteFileX(my::JoinFoldersToPath(directory, file));
}

void DeleteIndexesForAbsentCountries(string const & directory,
                                     int64_t version,
                                     function<bool(string const & filename)> const & isCountryName)
{
  vector<LocalCountryFile> files;
  FindAllLocalMapsInDirectory(directory, version, files);

  unordered_set<string> names;
  for (auto const & file : files)
    names.insert(file.GetCountryName());

  Platform::FilesList subdirs;
  Platform::GetFilesByType(directory, Platform::FILE_TYPE_DIRECTORY, subdirs);
  for (auto const & subdir : subdirs)
  {
    if (subdir == "." || subdir == "..")
      continue;
    if (!isCountryName(subdir) || names.count(subdir) != 0)
      continue;

    LocalCountryFile absentCountry(directory, CountryFile(subdir), version);
    CountryIndexes::DeleteFromDisk(absentCountry);
  }
}
}  // namespace

void DeleteDownloaderFilesForCountry(CountryFile const & countryFile, int64_t version)
{
  for (MapOptions file : {MapOptions::Map, MapOptions::CarRouting})
  {
    string const path = GetFileDownloadPath(countryFile, file, version);
    ASSERT(strings::EndsWith(path, READY_FILE_EXTENSION), ());
    my::DeleteFileX(path);
    my::DeleteFileX(path + RESUME_FILE_EXTENSION);
    my::DeleteFileX(path + DOWNLOADING_FILE_EXTENSION);
  }
}

void CleanupMapsDirectory(int64_t latestVersion,
                          function<bool(string const & filename)> const & isCountryName)
{
  Platform & platform = GetPlatform();

  string const mapsDir = platform.WritableDir();

  {
    // Delete Brazil.mwm and Japan.mwm maps, because they was replaces with
    // smaler regions after osrm routing implementation.
    vector<LocalCountryFile> localFiles;
    FindAllLocalMapsInDirectory(mapsDir, 0 /* version */, localFiles);
    for (LocalCountryFile & localFile : localFiles)
    {
      string const & countryName = localFile.GetCountryFile().GetNameWithoutExt();
      if (countryName == "Japan" || countryName == "Brazil")
      {
        localFile.SyncWithDisk();
        localFile.DeleteFromDisk(MapOptions::MapWithCarRouting);
      }
    }
  }

  // Try to delete empty folders.
  Platform::FilesList subdirs;
  platform.GetFilesByType(mapsDir, Platform::FILE_TYPE_DIRECTORY, subdirs);
  for (string const & subdir : subdirs)
  {
    // No need to visit parent directory.
    if (subdir == "..")
      continue;

    int64_t version = 0;
    if (subdir != "." && !ParseVersion(subdir, version))
      continue;

    string const subdirPath = my::JoinFoldersToPath(mapsDir, subdir);

    // It's OK to remove all temprorary files for maps older than app.
    if (version != latestVersion)
      DeleteDownloaderFilesForAllCountries(subdirPath);

    // It's OK to remove indexes for absent countries.
    DeleteIndexesForAbsentCountries(subdirPath, version, isCountryName);

    // Remove subdirectory if it does not contain any files except "." and "..".
    if (subdir != "." && Platform::IsDirectoryEmpty(subdirPath))
    {
      Platform::EError const ret = Platform::RmDir(subdirPath);
      ASSERT_EQUAL(Platform::ERR_OK, ret,
                   ("Can't remove empty directory:", subdirPath, "error:", ret));
      UNUSED_VALUE(ret);
    }
  }
}

void FindAllLocalMapsInDirectory(string const & directory, int64_t version,
                                 vector<LocalCountryFile> & localFiles)
{
  vector<string> files;
  Platform & platform = GetPlatform();

  platform.GetFilesByRegExp(directory, ".*\\" DATA_FILE_EXTENSION "$", files);
  for (string const & file : files)
  {
    // Remove DATA_FILE_EXTENSION and use base name as a country file name.
    string name = file;
    my::GetNameWithoutExt(name);
    localFiles.emplace_back(directory, CountryFile(name), version);
  }
}

void FindAllLocalMaps(vector<LocalCountryFile> & localFiles)
{
  localFiles.clear();

  Platform & platform = GetPlatform();

  string const directory = platform.WritableDir();
  FindAllLocalMapsInDirectory(directory, 0 /* version */, localFiles);

  Platform::FilesList subdirs;
  Platform::GetFilesByType(directory, Platform::FILE_TYPE_DIRECTORY, subdirs);
  for (string const & subdir : subdirs)
  {
    int64_t version;
    if (ParseVersion(subdir, version))
      FindAllLocalMapsInDirectory(my::JoinFoldersToPath(directory, subdir), version, localFiles);
  }

  // World and WorldCoasts can be stored in app bundle or in resources
  // directory, thus it's better to get them via Platform.
  for (string const & file : { WORLD_FILE_NAME, WORLD_COASTS_FILE_NAME })
  {
    auto i = localFiles.begin();
    for (; i != localFiles.end(); ++i)
    {
      if (i->GetCountryFile().GetNameWithoutExt() == file)
        break;
    }

    try
    {
      ModelReaderPtr reader(platform.GetReader(file + DATA_FILE_EXTENSION, GetSpecialFilesSearchScope()));

      // Assume that empty path means the resource file.
      LocalCountryFile worldFile(string(), CountryFile(file), version::ReadVersionTimestamp(reader));
      worldFile.m_files = MapOptions::Map;
      if (i != localFiles.end())
      {
        // Always use resource World files instead of local on disk.
        *i = worldFile;
      }
      else
        localFiles.push_back(worldFile);
    }
    catch (RootException const & ex)
    {
      if (i == localFiles.end())
      {
        // This warning is possible on android devices without pre-downloaded Worlds/fonts files.
        LOG(LWARNING, ("Can't find any:", file, "Reason:", ex.Msg()));
      }
    }
  }
}

bool ParseVersion(string const & s, int64_t & version)
{
  if (s.empty() || s.size() > kMaxTimestampLength)
    return false;

  int64_t v = 0;
  for (char const c : s)
  {
    if (!isdigit(c))
      return false;
    v = v * 10 + c - '0';
  }
  version = v;
  return true;
}

shared_ptr<LocalCountryFile> PreparePlaceForCountryFiles(CountryFile const & countryFile,
                                                         int64_t version)
{
  Platform & platform = GetPlatform();
  if (version == 0)
    return make_shared<LocalCountryFile>(platform.WritableDir(), countryFile, version);
  string const directory =
      my::JoinFoldersToPath(platform.WritableDir(), strings::to_string(version));
  if (!MkDirChecked(directory))
    return shared_ptr<LocalCountryFile>();
  return make_shared<LocalCountryFile>(directory, countryFile, version);
}

string GetFileDownloadPath(CountryFile const & countryFile, MapOptions file, int64_t version)
{
  Platform & platform = GetPlatform();
  string const readyFile = countryFile.GetNameWithExt(file) + READY_FILE_EXTENSION;
  if (version == 0)
    return my::JoinFoldersToPath(platform.WritableDir(), readyFile);
  return my::JoinFoldersToPath({platform.WritableDir(), strings::to_string(version)}, readyFile);
}

ModelReader * GetCountryReader(platform::LocalCountryFile const & file, MapOptions options)
{
  Platform & platform = GetPlatform();
  // See LocalCountryFile comment for explanation.
  if (file.GetDirectory().empty())
    return platform.GetReader(file.GetCountryName() + DATA_FILE_EXTENSION, GetSpecialFilesSearchScope());
  return platform.GetReader(file.GetPath(options), "f");
}

// static
void CountryIndexes::PreparePlaceOnDisk(LocalCountryFile const & localFile)
{
  string const dir = IndexesDir(localFile);
  if (!MkDirChecked(dir))
    MYTHROW(FileSystemException, ("Can't create directory", dir));
}

// static
bool CountryIndexes::DeleteFromDisk(LocalCountryFile const & localFile)
{
  string const directory = IndexesDir(localFile);
  bool ok = true;

  for (auto index : {Index::Bits, Index::Nodes, Index::Offsets})
  {
    string const path = GetPath(localFile, index);
    if (Platform::IsFileExistsByFullPath(path) && !my::DeleteFileX(path))
    {
      LOG(LERROR, ("Can't remove country index:", path));
      ok = false;
    }
  }

  Platform::EError const ret = Platform::RmDir(directory);
  if (ret != Platform::ERR_OK && ret != Platform::ERR_FILE_DOES_NOT_EXIST)
  {
    LOG(LERROR, ("Can't remove indexes directory:", directory, ret));
    ok = false;
  }
  return ok;
}

// static
string CountryIndexes::GetPath(LocalCountryFile const & localFile, Index index)
{
  char const * ext = nullptr;
  switch (index)
  {
    case Index::Bits:
      ext = kBitsExt;
      break;
    case Index::Nodes:
      ext = kNodesExt;
      break;
    case Index::Offsets:
      ext = kOffsetsExt;
      break;
  }
  return my::JoinFoldersToPath(IndexesDir(localFile), localFile.GetCountryName() + ext);
}

// static
void CountryIndexes::GetIndexesExts(vector<string> & exts)
{
  exts.push_back(kBitsExt);
  exts.push_back(kNodesExt);
  exts.push_back(kOffsetsExt);
}

// static
string CountryIndexes::IndexesDir(LocalCountryFile const & localFile)
{
  string dir = localFile.GetDirectory();
  CountryFile const & file = localFile.GetCountryFile();

  /// @todo It's a temporary code until we will put fIndex->fOffset into mwm container.
  /// IndexesDir should not throw any exceptions.
  if (dir.empty())
  {
    // Local file is stored in resources. Need to prepare index folder in the writable directory.
    int64_t const version = localFile.GetVersion();
    ASSERT_GREATER(version, 0, ());

    dir = my::JoinFoldersToPath(GetPlatform().WritableDir(), strings::to_string(version));
    if (!MkDirChecked(dir))
      MYTHROW(FileSystemException, ("Can't create directory", dir));
  }

  return my::JoinFoldersToPath(dir, file.GetNameWithoutExt());
}

string DebugPrint(CountryIndexes::Index index)
{
  switch (index)
  {
    case CountryIndexes::Index::Bits:
      return "Bits";
    case CountryIndexes::Index::Nodes:
      return "Nodes";
    case CountryIndexes::Index::Offsets:
      return "Offsets";
  }
}
}  // namespace platform
