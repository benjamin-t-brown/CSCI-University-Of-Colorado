echo \n\n
echo BenJammin's custom gdb script\n
echo Set breakpoint for phases and anti-explosion safeguard...\n

echo explode_bomb; 
break explode_bomb

echo \n
echo Display next machine instruction:
display /i $eip
echo\n
