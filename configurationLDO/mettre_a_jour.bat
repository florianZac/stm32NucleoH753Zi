@echo off
REM ===========================================================================
REM  mettre_a_jour.bat - met la carte a jour apres modification de ldo_config.csv
REM  1) verifie le CSV et genere les fichiers C (generer_config.py)
REM  2) compile le projet sans ouvrir CubeIDE (mode headless)
REM  3) flashe la carte par la sonde ST-LINK (STM32CubeProgrammer)
REM  A REGLER UNE FOIS : les 3 chemins ci-dessous (dossier d'installation de
REM  CubeIDE, de CubeProgrammer, et workspace CubeIDE contenant le projet).
REM  CubeIDE doit etre FERME (le workspace ne peut pas etre ouvert deux fois).
REM ===========================================================================
setlocal
set "CUBEIDE=C:\ST\STM32CubeIDE_1.16.0\STM32CubeIDE\stm32cubeidec.exe"
set "PROGRAMMER=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
set "WORKSPACE=%USERPROFILE%\STM32CubeIDE\workspace_1.16.0"
set "PROJET=Lwip_demo_H753"
set "CONFIG=Debug"

set "ICI=%~dp0"
set "RACINE=%ICI%.."

echo.
echo [1/3] Verification du CSV et generation...
python "%ICI%generer_config.py"
if errorlevel 1 goto erreur

echo.
echo [2/3] Compilation de %PROJET%/%CONFIG%...
if not exist "%CUBEIDE%" (echo Chemin CUBEIDE introuvable : %CUBEIDE% & goto erreur)
"%CUBEIDE%" --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data "%WORKSPACE%" -build "%PROJET%/%CONFIG%"
if errorlevel 1 goto erreur

echo.
echo [3/3] Programmation de la carte...
if not exist "%PROGRAMMER%" (echo Chemin PROGRAMMER introuvable : %PROGRAMMER% & goto erreur)
"%PROGRAMMER%" -c port=SWD -w "%RACINE%\%CONFIG%\%PROJET%.elf" -v -rst
if errorlevel 1 goto erreur

echo.
echo Carte a jour. Verifier dans la page /dac/config : "Carte a jour".
pause
exit /b 0

:erreur
echo.
echo ECHEC : rien n'a ete flashe (ou flash incomplet). Lire le message ci-dessus.
pause
exit /b 1
