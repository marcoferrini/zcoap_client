# TESTS
  here's a list of the test cases supported
  
  LEGEND
    * [+] fully supported 
    * [-] partially supported
  
  * [+]perform GET transaction (CON mode)
  * [+]Perform POST transaction (CON mode)
  * [+]Perform PUT transaction (CON mode)
  * [+]Perform DELETE transaction (CON mode)
  * [+]Perform GET transaction (NON mode)
  * [+]Perform POST transaction (NON mode)
  * [+]Perform PUT transaction (NON mode)
  * [+]Perform DELETE transaction (NON mode)
  * [-]Handle GET blockwise transfer for large resource (early negotiation)
  * [+]Handle PUT blockwise transfer for large resource
  * [+]Handle resource observation
  * [-]Handle resource observation with blockwise trasfer


## BUGS
  * the program crashes when doing a block wise transfer with a payload larger than 143 bytes (size of the entire payload not of each block)
