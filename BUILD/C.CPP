/****************************************************************************
*
*  Enhanced Program Maintenance Utility
*
*  Copyright (C) 1996, Michael O'Brien.  All rights reserved.
*
***/

#include <windows.h>
#include <stdio.h>

#define  WHITESPACE  " ,#=\t\n\r"

typedef struct _DEPENDENCY {
    char         filename[MAX_PATH];
    FILETIME     filetime;
    _DEPENDENCY *next;
} DEPENDENCY, *DEPENDENCYPTR;

typedef struct _VAR {
    char  name[256];
    char  value[256];
    _VAR *next;
} VAR, *VARPTR;

DEPENDENCYPTR dependencyhead       = NULL;
BOOL          optdebug             = 0;
BOOL          optrebuildall        = 0;
BOOL          optverbose           = 0;
char          scriptfile[MAX_PATH] = "";
VARPTR        varhead              = NULL;

BOOL  DoesFileMatchList (LPCSTR filename, LPCSTR list);
BOOL  FindIncludeFile (LPCSTR filename, BOOL searchlocal, LPSTR buffer, DWORD bufferlength);
BOOL  GetDependencyTime (LPCSTR filename, LPFILETIME filetime);
void  GetFileTimeByName (LPCSTR filename, FILETIME *filetime);
LPSTR GetVariableValue (LPCSTR name);
void  SetDependencyTime (LPCSTR filename, LPFILETIME filetime);
void  SetVariable (LPCSTR name, LPCSTR value);
void  StripExtension (LPSTR buffer);
void  StripFileName (LPSTR buffer);
LPSTR SubstituteVariables (LPSTR string, LPCSTR onlyvarname = NULL, int level = 0);
char *Tokenize (char *string);
void  Untokenize (char *token);

//===========================================================================
void CheckDependencies (LPCSTR filename, FILETIME *filetime) {

  // IF WE HAVE ALREADY DETERMINED THE TIME FOR THIS FILE AND ALL ITS
  // DEPENDENCIES, RETURN IT NOW
  {
    FILETIME thisfiletime = {0,0};
    if (GetDependencyTime(filename,&thisfiletime)) {
      if (CompareFileTime(&thisfiletime,filetime) > 0)
        CopyMemory(filetime,&thisfiletime,sizeof(FILETIME));
      return;
    }
  }

  // FIND THE FILE TIME FOR THIS FILE
  FILETIME resulttime = {0,0};
  GetFileTimeByName(filename,&resulttime);

  // SET A PRELIMINARY RESULT TIME FOR THIS FILE TO PREVENT RECURSING BACK
  // INTO THIS FILE
  SetDependencyTime(filename,&resulttime);

  // SET THE CURRENT DIRECTORY TO THIS FILE'S DIRECTORY
  char olddir[MAX_PATH] = "";
  {
    char fullpath[MAX_PATH] = "";
    char drive              = 0;
    char dir[MAX_PATH]      = "";
    _fullpath(fullpath,filename,MAX_PATH);
    _splitpath(fullpath,&drive,dir,NULL,NULL);
    wsprintf(fullpath,"%c:%s",drive,dir);
    GetCurrentDirectory(MAX_PATH,olddir);
    SetCurrentDirectory(fullpath);
  }

  // SEARCH THE FILE
  {
    const char *name = filename;
    while (strchr(name,'\\'))
      name = strchr(name,'\\')+1;
    FILE *f = fopen(name,"rt");
    while (f && !feof(f)) {

      // READ THE NEXT LINE
      char line[256] = "";
      fgets(line,254,f);
      char *curr = line;

      // REMOVE WHITESPACE
      while ((*curr == ' ') || (*curr == '\t'))
        ++curr;
      if (!*curr)
        continue;

      // CHECK FOR AN INCLUDE DIRECTIVE
      if (!_strnicmp(curr,"#include",8))
        curr += 8;
      else
        continue;

      // GET THE FILENAME INSIDE THE INCLUDE DIRECTIVE
      curr = strpbrk(curr,"\"<");
      if (!curr)
        continue;
      BOOL searchlocal = (*curr == '\"');
      ++curr;
      if (strpbrk(curr,"\">"))
        *strpbrk(curr,"\">") = 0;
      else
        continue;

      // GET THE FULL PATH NAME OF THE INCLUDE FILE
      char includefile[MAX_PATH] = "";
      if (!FindIncludeFile(curr,searchlocal,includefile,MAX_PATH))
        continue;

      // IF THIS INCLUDE FILE IS IN THE LIST OF DEPENDENCIES TO BE IGNORED,
      // SKIP ON
      if (DoesFileMatchList(includefile,GetVariableValue("-ignoredependencies")))
        continue;

      // SEARCH THE INCLUDE FILE
      CheckDependencies(includefile,&resulttime);

    }
    fclose(f);
  }

  // RESTORE THE CURRENT DIRECTORY
  SetCurrentDirectory(olddir);

  // SET THE FINALIZED RESULT TIME FOR THIS FILE
  SetDependencyTime(filename,&resulttime);

  // IF THE RESULT TIME IS GREATER THAN THE PASSED TIME, RETURN IT
  if (CompareFileTime(&resulttime,filetime) > 0)
    CopyMemory(filetime,&resulttime,sizeof(FILETIME));

}

//===========================================================================
void DeleteVariables () {
  while (varhead) {
    VARPTR next = varhead->next;
    free(varhead);
    varhead = next;
  }
}

//===========================================================================
void DisplayCopyrightNotice () {
  printf("\n"
         "Enhanced Program Maintenance Utility v1.01\n"
         "Copyright (C) 1996, Michael O'Brien.  All rights reserved.\n\n");
}

//===========================================================================
void DisplayHelp () {
  printf("Usage: C [options] [project.cs]\n"
         "\n"
         "Options:\n"
         "\n"
         "/A Rebuild all\n"
         "/D Debug build\n"
         "/O Optimized build (default)\n"
         "/V Verbose mode\n"
         "\n");
}

//===========================================================================
BOOL DoesFileMatchList (LPCSTR filename, LPCSTR list) {
  if (!(filename && list))
    return 0;
  char locallist[256] = "";
  strcpy(locallist,list);
  char *curr = strtok(locallist," \t;");
  while (curr) {
    if ((strlen(filename) >= strlen(curr)) &&
        !_stricmp(filename+strlen(filename)-strlen(curr),curr))
      return 1;
    curr = strtok(NULL," \t;");
  }
  return 0;
}

//===========================================================================
BOOL ExecuteCommand (LPCSTR cmdline, BOOL shellcommand) {
  if (!*cmdline)
    return 1;
  if (optverbose)
    printf("%s\n",cmdline);
  char localcommand[MAX_PATH] = "";
  if (shellcommand) {
    GetEnvironmentVariable("comspec",localcommand,MAX_PATH);
    strcat(localcommand," /c ");
  }
  strcat(localcommand,cmdline);
  STARTUPINFO         startupinfo;
  PROCESS_INFORMATION processinfo;
  ZeroMemory(&startupinfo,sizeof(STARTUPINFO));
  ZeroMemory(&processinfo,sizeof(PROCESS_INFORMATION));
  startupinfo.cb = sizeof(STARTUPINFO);
  if (!CreateProcess(NULL,
                     localcommand,
                     (LPSECURITY_ATTRIBUTES)NULL,
                     (LPSECURITY_ATTRIBUTES)NULL,
                     0,
                     0,
                     NULL,
                     NULL,
                     &startupinfo,
                     &processinfo))
    return 0;
  WaitForSingleObject(processinfo.hProcess,INFINITE);
  DWORD exitcode = 0;
  GetExitCodeProcess(processinfo.hProcess,&exitcode);
  CloseHandle(processinfo.hThread);
  CloseHandle(processinfo.hProcess);
  return !exitcode;
}

//===========================================================================
BOOL FindIncludeFile (LPCSTR filename, BOOL searchlocal, LPSTR buffer, DWORD bufferlength) {
  char    *findname = NULL;
  OFSTRUCT of;

  // SEARCH IN THE CURRENT DIRECTORY IF ALLOWED
  if (searchlocal && SearchPath(".",filename,NULL,bufferlength,buffer,&findname))
    if (OpenFile(buffer,&of,OF_EXIST) != HFILE_ERROR)
      return 1;

  // SEARCH IN THE C.EXE PROGRAM DIRECTORY
  {
    char cpath[MAX_PATH] = "";
    GetModuleFileName(GetModuleHandle(NULL),cpath,MAX_PATH);
    StripFileName(cpath);
    if (SearchPath(cpath,filename,NULL,bufferlength,buffer,&findname))
      if (OpenFile(buffer,&of,OF_EXIST) != HFILE_ERROR)
        return 1;
  }

  // SEARCH IN ALL THE DIRECTORIES IN THE INCLUDE PATH
  {
    char includepath[MAX_PATH] = "";
    GetEnvironmentVariable("include",includepath,MAX_PATH);
    if (SearchPath(includepath,filename,NULL,bufferlength,buffer,&findname))
      if (OpenFile(buffer,&of,OF_EXIST) != HFILE_ERROR)
        return 1;
  }

  return 0;
}

//===========================================================================
BOOL GetDependencyTime (LPCSTR filename, LPFILETIME filetime) {
  DEPENDENCYPTR curr = dependencyhead;
  while (curr && _stricmp(curr->filename,filename))
    curr = curr->next;
  if (curr)
    CopyMemory(filetime,&curr->filetime,sizeof(FILETIME));
  else
    ZeroMemory(filetime,sizeof(FILETIME));
  return (curr != NULL);
}

//===========================================================================
void GetFileTimeByName (LPCSTR filename, FILETIME *filetime) {
  HANDLE outputhandle = CreateFile(filename,
                                   GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   (LPSECURITY_ATTRIBUTES)NULL,
                                   OPEN_EXISTING,
                                   0,
                                   NULL);
  GetFileTime(outputhandle,NULL,NULL,filetime);
  CloseHandle(outputhandle);
}

//===========================================================================
LPSTR GetVariableValue (LPCSTR name) {
  VARPTR currvar = varhead;
  while (currvar)
    if (!_stricmp(name,currvar->name))
      return currvar->value;
    else
      currvar = currvar->next;
  return NULL;
}

//===========================================================================
BOOL IsTargetOutOfDate (LPCSTR sourcefilename, LPCSTR targetfilename) {

  // GET THE FILE TIME OF THE TARGET FILE.  IF THE TARGET FILE DOES
  // NOT EXIST, RETURN TRUE.
  FILETIME targetfiletime = {0,0};
  GetFileTimeByName(targetfilename,&targetfiletime);
  if (!(targetfiletime.dwLowDateTime || targetfiletime.dwHighDateTime))
    return 1;

  // GET THE FILE TIME OF THE SOURCE FILE.  IF THE SOURCE FILE IS MORE
  // RECENT THAN THE TARGET FILE, RETURN TRUE.
  FILETIME sourcefiletime = {0,0};
  GetFileTimeByName(sourcefilename,&sourcefiletime);
  if (CompareFileTime(&sourcefiletime,&targetfiletime) > 0)
    return 1;

  // DO AUTO DEPENDENCY CHECKING
  if (DoesFileMatchList(sourcefilename,GetVariableValue("-autodependencies"))) {
    FILETIME dependencyfiletime = {0,0};
    CheckDependencies(sourcefilename,&dependencyfiletime);
    if (CompareFileTime(&dependencyfiletime,&targetfiletime) > 0)
      return 1;
  }

  return 0;
}

//===========================================================================
BOOL ProcessCommandLine (int argc, char *argv[]) {

  // PROCESS COMMAND LINE ARGUMENTS
  {
    for (int loop = 1; loop < argc; ++loop)
      if ((argv[loop][0] == '-') || (argv[loop][0] == '/')) {
        char *curr = argv[loop]+1;
        while (*curr)
          switch ((char)LOBYTE(CharUpper((char *)*(curr++)))) {
            case '?': return 0;
            case 'H': return 0;
            case 'A': optrebuildall = 1;  break;
            case 'D': optdebug      = 1;  break;
            case 'O': optdebug      = 0;  break;
            case 'V': optverbose    = 1;  break;
          }
      }
      else if (!scriptfile[0])
        strcpy(scriptfile,argv[loop]);
  }

  // IF WE WERE GIVEN A PROJECT NAME, VERIFY THAT IT EXISTS
  if (scriptfile[0]) {
    OFSTRUCT of;
    if (OpenFile(scriptfile,&of,OF_EXIST) == HFILE_ERROR) {
      strcat(scriptfile,".cs");
      if (OpenFile(scriptfile,&of,OF_EXIST) == HFILE_ERROR)
        return 0;
    }
  }

  // IF WE WEREN'T GIVEN A PROJECT NAME, DETERMINE IT BY SEARCHING FOR A
  // COMPILE SCRIPT (.CS) FILE IN THIS DIRECTORY
  if (!scriptfile[0]) {
    WIN32_FIND_DATA finddata;
    HANDLE          findhandle = FindFirstFile("*.cs",&finddata);
    if (findhandle != INVALID_HANDLE_VALUE) {
      do
        if ((!scriptfile[0]) ||
            (_stricmp(finddata.cFileName,scriptfile) < 0))
          strcpy(scriptfile,finddata.cFileName);
      while (FindNextFile(findhandle,&finddata));
      FindClose(findhandle);
    }
  }

  // IF WE COULDN'T FIND A SCRIPT FILE, RETURN FAILURE
  if (!scriptfile[0])
    return 0;

  // ADD A VARIABLE FOR THE PROJECT NAME (WITHOUT THE .CS EXTENSION)
  {
    char buffer[MAX_PATH];
    strcpy(buffer,scriptfile);
    StripExtension(buffer);
    SetVariable("project",buffer);
  }

  return 1;
}

//===========================================================================
BOOL ProcessScript (LPCSTR filename, int pass, LPCSTR label) {
  FILE *f = fopen(filename,"rt");
  if (!f)
    return 0;
  while (!feof(f)) {

    // READ THE NEXT LINE
    char line[256] = "";
    fgets(line,254,f);
    char *curr = line;

    // REMOVE WHITESPACE
    while ((*curr == ' ') || (*curr == '\t'))
      ++curr;
    if (strchr(curr,'\n'))
      *strchr(curr,'\n') = 0;
    if (strchr(curr,'\r'))
      *strchr(curr,'\r') = 0;
    if (!*curr)
      continue;

    // IF WE ARE SEARCHING FOR A LABEL, CHECK FOR A MATCH AND THEN SKIP ON
    if (label) {
      if ((*curr == ':') && !_stricmp(curr+1,label))
        label = NULL;
      continue;
    }

    // IF THIS LINE IS A COMMENT, SKIP IT
    if (strchr(";:/",*curr))
      continue;

    // IF THIS LINE IS NOT TO BE PROCESSED IN THIS PASS, SKIP IT
    if ((*curr != '#') && ((*curr == '!') != pass))
      continue;
    if (*curr == '!')
      ++curr;

    // START THE TOKEN SEARCH
    curr = Tokenize(curr);

    // PROCESS INCLUDE DIRECTIVES
    if (!_stricmp(curr,"include")) {
      curr = Tokenize(NULL);
      BOOL local = 1;
      if (*curr == '<') {
        local = 0;
        ++curr;
        if (strchr(curr,'>'))
          *strchr(curr,'>') = 0;
      }
      char scriptpath[MAX_PATH]  = "";
      char currentpath[MAX_PATH] = "";
      char newscript[MAX_PATH]   = "";
      strcpy(scriptpath,filename);
      StripFileName(scriptpath);
      GetCurrentDirectory(MAX_PATH,currentpath);
      SetCurrentDirectory(scriptpath);
      BOOL success = FindIncludeFile(curr,local,newscript,MAX_PATH);
      SetCurrentDirectory(currentpath);
      if (success) {
        if (!ProcessScript(newscript,pass,NULL)) {
          fclose(f);
          return 0;
        }
      }
      else {
        printf("ERROR: Unable to find include file %s\n",curr);
        fclose(f);
        return 0;
      }
      continue;
    }

    // PROCESS IF STATEMENTS
    if (!_stricmp(curr,"if")) {

      // DETERMINE WHETHER THE CONDITION IS TRUE
      BOOL true = 0;
      char *equals = strchr(curr+strlen(curr)+1,'=');
      curr = Tokenize(NULL);
      if (!_stricmp(curr,"not")) {
        true = 1;
        curr = Tokenize(NULL);
      }
      if (!_stricmp(curr,"exist")) {
        curr = Tokenize(NULL);
        curr = SubstituteVariables(curr);
        WIN32_FIND_DATA finddata;
        HANDLE          findhandle = FindFirstFile(curr,&finddata);
        if (findhandle != INVALID_HANDLE_VALUE) {
          true = !true;
          FindClose(findhandle);
        }
        curr = Tokenize(NULL);
      }
      else {
        curr = SubstituteVariables(curr);
        char *next = Tokenize(NULL);
        if (equals && (next > equals)) {
          if (!_stricmp(curr,next))
            true = !true;
          curr = Tokenize(NULL);
        }
        else {
          if (*curr)
            true = !true;
          curr = next;
        }
      }

      // IF THE CONDITION IS NOT TRUE, SKIP THIS LINE
      if (!true)
        continue;

      // OTHERWISE, CONTINUE PROCESSING
    }

    // PROCESS ECHO STATEMENTS
    if (!_stricmp(curr,"echo")) {
      curr = Tokenize(NULL);
      Untokenize(curr);
      curr = SubstituteVariables(curr);
      printf("%s\n",curr);
      continue;
    }

    // PROCESS GOTO STATEMENTS
    if (!_stricmp(curr,"goto")) {
      curr = Tokenize(NULL);
      curr = SubstituteVariables(curr);
      fclose(f);
      char label[256];
      strcpy(label,curr);
      return ProcessScript(filename,pass,label);
    }

    // PROCESS HALT STATEMENTS
    if (!_stricmp(curr,"halt")) {
      curr = Tokenize(NULL);
      Untokenize(curr);
      if (*curr) {
        curr = SubstituteVariables(curr);
        printf("%s\n",curr);
      }
      fclose(f);
      return 0;
    }

    // PROCESS SET STATEMENTS
    if (!_stricmp(curr,"set")) {
      char *name  = Tokenize(NULL);
      curr = SubstituteVariables(curr,name);
      char *value = Tokenize(NULL);
      Untokenize(value);
      if (value && *value)
        value = SubstituteVariables(value,name);
      if (name && *name)
        SetVariable(name,(value && *value) ? value : " ");
      continue;
    }

    // PROCESS COMMANDS
    Untokenize(curr);
    curr = SubstituteVariables(curr);
    if (!ExecuteCommand(curr,1)) {
      fclose(f);
      return 0;
    }

  }
  fclose(f);
  return !label;
}

//===========================================================================
BOOL ProcessSourceFiles () {
  VARPTR currtype = varhead;
  while (currtype) {
    if ((currtype->name[0] == '.') && !strchr(currtype->name+1,'.')) {

      // FIND THE BUILD COMMAND FOR THIS TYPE
      VARPTR currcmd = varhead;
      while (currcmd &&
             ((strlen(currcmd->name) <= strlen(currtype->name)) ||
              _strnicmp(currcmd->name,currtype->name,strlen(currtype->name)) ||
              _stricmp(currcmd->name+strlen(currtype->name),currtype->value)))
        currcmd = currcmd->next;
      if (!currcmd) {
        printf("ERROR: No command specified for %s\n",currtype->name);
        return 0;
      }

      // DETERMINE WHETHER THE BUILD COMMAND PROCESSES ONE FILE AT A TIME
      // OR PROCESSES A WHOLE BATCH AT ONCE
      BOOL eachfile = 0;
      {
        char *curr = currcmd->value;
        while (strchr(curr,'%')) {
          curr = strchr(curr,'%');
          if (!_strnicmp(curr,"%file%",6))
            eachfile = 1;
          ++curr;
        }
      }

      // ITERATE THROUGH THE FILES, PROCESSING EACH ONE OR CHECKING TO SEE
      // IF ANY ARE OUT OF DATE
      BOOL anyoutofdate = 0;
      char filespec[MAX_PATH];
      wsprintf(filespec,"*%s",currtype->name);
      WIN32_FIND_DATA finddata;
      HANDLE          findhandle = FindFirstFile(filespec,&finddata);
      if (findhandle != INVALID_HANDLE_VALUE) {
        do {
          char targetfile[MAX_PATH] = "";
          if (eachfile) {
            strcpy(targetfile,finddata.cFileName);
            StripExtension(targetfile);
            strcat(targetfile,currtype->value);
          }
          else {
            char *varvalue = GetVariableValue("project");
            if (varvalue) {
              strcpy(targetfile,varvalue);
              strcat(targetfile,currtype->value);
            }
          }
          if (optrebuildall || IsTargetOutOfDate(finddata.cFileName,targetfile))
            if (eachfile) {
              SetVariable("file",finddata.cFileName);
              if (!ExecuteCommand(SubstituteVariables(currcmd->value),0)) {
                FindClose(findhandle);
                return 0;
              }
              SetVariable("file","");
            }
          else
            anyoutofdate = 1;
        } while (FindNextFile(findhandle,&finddata));
        FindClose(findhandle);
      }

      // IF THE BUILD COMMAND WORKS ON A WHOLE BATCH OF FILES, AND ANY
      // FILES WERE OUT OF DATE, RUN THE BUILD COMMAND
      if (anyoutofdate)
        if (!ExecuteCommand(SubstituteVariables(currcmd->value),0))
          return 0;

    }
    currtype = currtype->next;
  }
  return 1;
}

//===========================================================================
void SetEnvironmentVariables () {
  LPSTR strings = (LPSTR)GetEnvironmentStrings();
  LPSTR curr    = strings;
  while (*curr) {
    if (*curr != '=') {
      char buffer[256];
      strcpy(buffer,curr);
      LPSTR name  = Tokenize(buffer);
      LPSTR value = Tokenize(NULL);
      SetVariable(name,value);
    }
    curr += strlen(curr)+1;
  }
  FreeEnvironmentStrings(strings);
}

//===========================================================================
void SetDependencyTime (LPCSTR filename, LPFILETIME filetime) {

  // IF THIS DEPENDENCY ALREADY EXISTS, JUST UPDATE THE TIME
  DEPENDENCYPTR curr = dependencyhead;
  while (curr && _stricmp(curr->filename,filename))
    curr = curr->next;
  if (curr) {
    CopyMemory(&curr->filetime,filetime,sizeof(FILETIME));
    return;
  }

  // OTHERWISE, CREATE A NEW RECORD
  DEPENDENCYPTR ptr = (DEPENDENCYPTR)malloc(sizeof(DEPENDENCY));
  if (ptr)
    ZeroMemory(ptr,sizeof(DEPENDENCY));
  else
    return;
  strcpy(ptr->filename,filename);
  CopyMemory(&ptr->filetime,filetime,sizeof(FILETIME));
  ptr->next      = dependencyhead;
  dependencyhead = ptr;

}

//===========================================================================
void SetVariable (LPCSTR name, LPCSTR value) {

  // IF THIS VARIABLE ALREADY EXISTS, REPLACE IT
  VARPTR currvar = varhead;
  while (currvar && _stricmp(currvar->name,name))
    currvar = currvar->next;
  if (currvar) {
    strcpy(currvar->value,value);
    return;
  }

  // OTHERWISE, CREATE A NEW VARIABLE
  VARPTR newvar = (VARPTR)malloc(sizeof(VAR));
  if (newvar)
    ZeroMemory(newvar,sizeof(VAR));
  else
    return;
  strcpy(newvar->name,name);
  strcpy(newvar->value,value);

  // ADD IT TO THE LIST
  VARPTR *next = &varhead;
  while (*next)
    next = &(*next)->next;
  *next = newvar;

}

//===========================================================================
void StripExtension (LPSTR buffer) {
  char *ext = buffer;
  while (strchr(ext,'\\'))
    ext=strchr(ext,'\\')+1;
  if (strchr(ext,'.'))
    *strchr(ext,'.') = 0;
}

//===========================================================================
void StripFileName (LPSTR buffer) {
  if (strchr(buffer,'\\')) {
    char *last = strchr(buffer,'\\')+1;
    while (strchr(last,'\\'))
      last = strchr(last,'\\')+1;
    *last = 0;
  }
  else if (strchr(buffer,':'))
    *(strchr(buffer,':')+1) = 0;
}

//===========================================================================
LPSTR SubstituteVariables (LPSTR string, LPCSTR onlyvarname, int level) {
  static char result[256];
  *result = 0;
  char *curr = result;
  while (strchr(string,'%')) {

    // COPY THE TEXT UNTIL THE NEXT VARIABLE
    strcpy(curr,string);
    *strchr(curr,'%') = 0;
    string += strlen(curr);
    curr   += strlen(curr);

    // IF THIS IS THE VARIABLE WE ARE SEARCHING FOR, OR IF WE ARE
    // PROCESSING ALL VARIABLES, THEN COPY THE VALUE OF THE VARIABLE
    char varname[256];
    strcpy(varname,string+1);
    if (strchr(varname,'%'))
      *strchr(varname,'%') = 0;
    if ((!onlyvarname) || !_stricmp(onlyvarname,varname)) {
      char *varvalue = GetVariableValue(varname);
      if (varvalue) {
        strcpy(curr,varvalue);
        string += strlen(varname)+2;
      }
      else
        if (strchr(string+1,'%'))
          string = strchr(string+1,'%')+1;
        else
          ++string;
      curr += strlen(curr);
    }

    // OTHERWISE, COPY THE VARIABLE NAME WITH BOUNDING PARENTHESIS
    else {
      sprintf(curr,"%%%s%%",varname);
      curr   += strlen(curr);
      string += strlen(varname)+2;
    }

  }
  strcpy(curr,string);
  if (strchr(result,'%') && (level < 16)) {
    char save[256];
    strcpy(save,result);
    return SubstituteVariables(save,onlyvarname,level+1);
  }
  return result;
}

//===========================================================================
char *Tokenize (char *string) {
  static char *savedpos = NULL;

  // IF WE WERE PASSED A STRING, USE IT.  OTHERWISE, SKIP PAST THE LAST
  // SAVED TOKEN.
  if (string) {
    *(string+strlen(string)+1) = 0;
    savedpos = string;
  }
  else
    if (savedpos)
      savedpos += strlen(savedpos)+1;
    else
      return NULL;

  // SKIP PAST LEADING WHITESPACE
  while ((*savedpos) && strchr(WHITESPACE,*savedpos))
    ++savedpos;

  // IF WE ARE AT THE END OF THE STRING, RETURN
  if (!*savedpos)
    return savedpos;

  // SEARCH FOR THE NEXT WHITESPACE CHARACTER AND CHANGE IT INTO A
  // STRING TERMINATOR
  {
    char *curr     = savedpos;
    BOOL  inquotes = 0;
    while (*curr)
      if ((!inquotes) && strchr(WHITESPACE,*curr))
        *curr = 0;
      else {
        if (*curr == '\"')
          inquotes = !inquotes;
        ++curr;
      }
  }

  // REMOVE QUOTES
  {
    char *curr = savedpos;
    while (*curr)
      if (*curr == '\"') {
        int length = strlen(curr);
        MoveMemory(curr,curr+1,length);
        *(curr+length) = ' ';
      }
      else if (*curr == '\\') {
        ++curr;
        if (*curr)
          ++curr;
      }
      else
        ++curr;
  }

  return savedpos;
}

//===========================================================================
void Untokenize (char *token) {
  if (*(token+strlen(token)+1))
    *(token+strlen(token)) = ' ';
  while ((*token) && strchr(WHITESPACE,*(token+strlen(token)-1)))
    *(token+strlen(token)-1) = 0;
}

//===========================================================================
int __cdecl main (int argc, char *argv[]) {
  SetEnvironmentVariables();
  if (!ProcessCommandLine(argc,argv)) {
    DisplayCopyrightNotice();
    DisplayHelp();
    DeleteVariables();
    return 0;
  }
  if (optdebug)
    SetVariable("debug"," ");
  if (ProcessScript(scriptfile,0,NULL))
    if (ProcessSourceFiles())
      if (ProcessScript(scriptfile,1,NULL))
        printf("[success]\n");
  DeleteVariables();
  return 0;
}
