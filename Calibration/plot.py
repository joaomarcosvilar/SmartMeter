import matplotlib.pyplot as plt

# Suponha que você tenha dados como estes
x = [1, 2, 3, 4, 5]  # Eixo X (tempo, por exemplo)
y = [10, 20, 15, 25, 30]  # Eixo Y (leitura de sensor, por exemplo)

# Cria o gráfico de linha
plt.plot(x, y, label='Leitura do Sensor')

# Adiciona título e rótulos
plt.title('Leitura do Sensor ao Longo do Tempo')
plt.xlabel('Tempo (s)')
plt.ylabel('Valor do Sensor')

# Adiciona uma legenda
plt.legend()

# Exibe o gráfico
plt.show()
