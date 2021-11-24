# Fda program
# Abril 17 de 2005
#
ppal: Fsa.h Fsa.cpp
	g++ -Wall Fsa.cpp -o oilnsym
	#echo "START!"
	#./oilnsym ejemplo.sample salida1.auto 0

valgrind: Fsa.h Fsa.cpp
	g++ -Wall -g -O0 Fsa.cpp -o oilnsym

runValgrind: valgrind
	valgrind --leak-check=yes ./fsaV ../target1/ ../target1/muestra_1000.sample ../target1/expVALGRIND_


