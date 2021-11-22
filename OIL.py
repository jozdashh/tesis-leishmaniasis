# Adaptar OIL para Python
# Autor: Josue Peña Atencio
# Ultima modificación: 19 de Julio 2021

from subprocess import Popen, PIPE

# filename_input: nombre de archivo de palabras de entrada
# filename_output: nombre de archivo de salida para guardar automata resultante
def OIL(filename_input, filename_output):
    process = Popen(["./oilnsym", filename_input, filename_output, "1"], stdout=PIPE)
    process.communicate()
    exit_code = process.wait()
