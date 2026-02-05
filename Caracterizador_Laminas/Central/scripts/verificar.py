import pyvisa

rm = pyvisa.ResourceManager('@py')
resources = rm.list_resources()
print("Recursos encontrados:", resources)
