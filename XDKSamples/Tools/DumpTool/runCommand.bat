echo Waiting for a Title process to start
xbconnect /wt

echo Copy over the current debug version of the tool to the default console
xbcp /x/title Durango\Debug\DumpTool.exe xd:\

echo Running the tool with args: %*
xbrun /x/title /O d:\DumpTool -pdt:triage %*
