@echo off
REM ===========================================================================
REM  generer_doc_conception.bat - documentation du firmware Lwip_demo_H753
REM  1) Doxygen lit les commentaires de main.c (+ conception.dox, complements_cubemx.dox)
REM     -> sortie\html\index.html (documentation navigable) et sortie\xml\
REM  2) doxygen_vers_docx.py transforme le XML en document Word
REM     -> sortie\Document_Conception_Firmware.docx
REM  Prerequis : Doxygen 1.18 et Python 3 ; Graphviz (dot) facultatif
REM  (graphes d'appel). Voir la notice Notice_Doxygen.docx.
REM ===========================================================================
setlocal
cd /d "%~dp0"

REM Doxygen : PATH, sinon dossier d'installation par defaut
set "DOXY=doxygen"
where doxygen >nul 2>&1
if not errorlevel 1 goto doxy_ok
set "DOXY=%ProgramFiles%\doxygen\bin\doxygen.exe"
if exist "%DOXY%" goto doxy_ok
echo Doxygen introuvable (ni dans le PATH, ni dans %ProgramFiles%\doxygen\bin).
echo Installer Doxygen : voir Notice_Doxygen.docx, etape 2.
goto erreur
:doxy_ok

REM Graphviz (facultatif) : PATH, sinon dossier d'installation par defaut
set "DOTDIR="
where dot >nul 2>&1
if not errorlevel 1 goto dot_ok
if exist "%ProgramFiles%\Graphviz\bin\dot.exe" set "DOTDIR=%ProgramFiles%\Graphviz\bin"
if defined DOTDIR goto dot_ok
set "DOTDIR=ABSENT"
:dot_ok

set "PY=python"
where python >nul 2>&1
if errorlevel 1 set "PY=py -3"

echo.
echo [1/3] Nettoyage de l'ancienne sortie...
if exist sortie rmdir /s /q sortie
mkdir sortie

echo.
echo [2/3] Doxygen...
if "%DOTDIR%"=="ABSENT" goto sans_dot
if "%DOTDIR%"=="" goto dot_path
(type Doxyfile & echo DOT_PATH = "%DOTDIR%") | "%DOXY%" -
goto doxy_fin
:dot_path
"%DOXY%" Doxyfile
goto doxy_fin
:sans_dot
echo Graphviz absent : graphes d'appel desactives pour cette generation.
(type Doxyfile & echo HAVE_DOT = NO) | "%DOXY%" -
:doxy_fin
if errorlevel 1 goto erreur
REM Avertissements utiles : on retire les deux messages attendus par la convention
REM du projet ("@param Aucun." et "@retval Aucun (void)." sur les fonctions void).
findstr /v /c:"found documented return type" /c:"of command @param is not found" sortie\avertissements.txt > sortie\avertissements_utiles.txt

echo.
echo [3/3] Document Word de conception...
%PY% doxygen_vers_docx.py
if errorlevel 1 goto erreur

echo.
echo Termine :
echo   sortie\html\index.html
echo   sortie\Document_Conception_Firmware.docx
echo   sortie\avertissements_utiles.txt (vide = commentaires corrects)
start "" "sortie\html\index.html"
pause
exit /b 0

:erreur
echo.
echo ECHEC - voir le message ci-dessus.
pause
exit /b 1
