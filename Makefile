# Definindo o compilador
CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11

# Diretório de saída
BUILD_DIR = ./build
IMG_DIR = ./media

# Arquivo principal
TARGET = main_app
SRC = main.cpp

# Caminho das bibliotecas (raiz do projeto)
LIB_PATH = .

# Bibliotecas necessárias do SDK 
HIKVISION_LIBS = -lhcnetsdk -lPlayCtrl -lAudioRender -lSuperRender

LIBS = $(HIKVISION_LIBS) -lhpr -lHCCore -lpthread -lHCCoreDevCfg -lStreamTransClient -lSystemTransform -lHCPreview -lHCAlarm -lHCGeneralCfgMgr -lHCIndustry -lHCPlayBack -lHCVoiceTalk -lanalyzedata -lHCDisplay

# Opção para incluir o caminho dinâmico das bibliotecas
LDFLAGS = -Wl,-rpath=$(LIB_PATH) -L$(LIB_PATH) $(LIBS)

# Incluir os arquivos de cabeçalho
INCLUDES = -I$(LIB_PATH)

# Regra padrão
all: $(BUILD_DIR)/$(TARGET)

# Regra para compilar o executável
$(BUILD_DIR)/$(TARGET): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Limpar os arquivos compilados
clean:
	@rm -rf $(BUILD_DIR)

# Criar a pasta de imagens, se não existir
prepare:
	@mkdir -p $(IMG_DIR)
