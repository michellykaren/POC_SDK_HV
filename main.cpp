#include "HCNetSDK.h"
#include "LinuxPlayM4.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <cstring>

// definir para ativar o teste de captura com N iterações
#define TESTE_TEMPO_CAPTURA  // comentar para executar o código normalmente
#ifdef TESTE_TEMPO_CAPTURA
    #define NUM_ITERACOES 100

    #include <chrono>
    #include <vector>
    #include <algorithm>
    #include <numeric>

#endif

// inicializar o SDK
bool initSDK() {
    if (!NET_DVR_Init()) {
        std::cerr << "Erro ao inicializar o SDK: " << NET_DVR_GetLastError() << std::endl;
        return false;
    }
    std::cout << "SDK inicializado com sucesso!" << std::endl;
    return true;
}

// opções de conexão
void setConnectionOptions() {
    NET_DVR_SetConnectTime(2000, 1);
    NET_DVR_SetReconnect(10000, true);
    std::cout << "Configurações de conexão ajustadas!" << std::endl;
}

LONG loginToCamera(const char* ip, WORD port, const char* username, const char* password) {
    NET_DVR_USER_LOGIN_INFO loginInfo = {0};
    NET_DVR_DEVICEINFO_V40 deviceInfo = {0};

    strncpy(loginInfo.sDeviceAddress, ip, sizeof(loginInfo.sDeviceAddress) - 1);
    strncpy(loginInfo.sUserName, username, sizeof(loginInfo.sUserName) - 1);
    strncpy(loginInfo.sPassword, password, sizeof(loginInfo.sPassword) - 1);
    loginInfo.wPort = port;
    loginInfo.bUseAsynLogin = false;

    LONG userID = NET_DVR_Login_V40(&loginInfo, &deviceInfo);
    if (userID < 0) {
        std::cerr << "Erro ao fazer login: " << NET_DVR_GetLastError() << std::endl;
    } else {
        std::cout << "Login bem-sucedido! ID do usuário: " << userID << " IP da câmera: " << ip << std::endl;
    }
    return userID;
}

// callback de dados de vídeo
void CALLBACK RealDataCallback_V30(LONG lRealHandle, DWORD dwDataType, BYTE* pBuffer, DWORD dwBufSize, void* pUser) {
    std::cerr << "Callback acionado. Tipo de dados: " << dwDataType << " Tamanho do buffer: " << dwBufSize << std::endl;
    LONG* pPort = (LONG*)pUser;

    if (!PlayM4_OpenStream(*pPort, pBuffer, dwBufSize, 1024 * 1024)) {
        std::cerr << "Erro ao abrir o stream no PlayM4." << std::endl;
        return;
    }

    if (dwDataType == NET_DVR_STREAMDATA && *pPort != -1 && dwBufSize > 0) {
        if (!PlayM4_InputData(*pPort, pBuffer, dwBufSize)) {
            int errorCode = PlayM4_GetLastError(*pPort);
            std::cerr << "Erro ao inserir dados no PlayM4. Código de erro: " << errorCode << std::endl;
        } else {
            std::cout << "Dados inseridos com sucesso no PlayM4!" << std::endl;
        }
    }
}

// stream de vídeo
LONG startVideoStream(LONG userID, LONG& nPort) {
    NET_DVR_PREVIEWINFO previewInfo = {0};
    previewInfo.lChannel = 1;
    previewInfo.dwStreamType = 0;
    previewInfo.dwLinkMode = 0;
    previewInfo.hPlayWnd = 0;
    previewInfo.bBlocked = 1;

    LONG playHandle = NET_DVR_RealPlay_V40(userID, &previewInfo, RealDataCallback_V30, &nPort);
    if (playHandle < 0) {
        std::cerr << "Erro ao iniciar o stream de vídeo: " << NET_DVR_GetLastError() << std::endl;
        return -1;
    }
    std::cout << "Stream de vídeo iniciado com sucesso!" << std::endl;

    if (!PlayM4_GetPort(&nPort)) {
        std::cerr << "Erro ao obter porta do PlayM4." << std::endl;
        return -1;
    }
    std::cout << "Porta obtida para PlayM4: " << nPort << std::endl;

    if (!PlayM4_SetStreamOpenMode(nPort, STREAME_REALTIME)) {
        std::cerr << "Erro ao configurar o modo de stream para real-time." << std::endl;
        return -1;
    }

    return playHandle;
}

// Função para capturar JPEG
bool captureJPEG(LONG nPort, const char* filePath) {
    std::this_thread::sleep_for(std::chrono::seconds(5)); // Aguarde alguns segundos para garantir que o stream esteja fluindo

    unsigned char* jpegBuffer = new unsigned char[50 * 1024 * 1024]; // Buffer de 50MB para armazenar o JPEG
    unsigned int jpegSize = 0;

    if (nPort < 0) {
        std::cerr << "Erro: Porta inválida para PlayM4." << std::endl;
        delete[] jpegBuffer;
        return false;
    }

    // Tentar capturar o JPEG
    if (!PlayM4_GetJPEG(nPort, jpegBuffer, 50 * 1024 * 1024, &jpegSize)) {
        int errorCode = PlayM4_GetLastError(nPort);
        std::cerr << "Erro ao capturar JPEG. Código de erro: " << errorCode << std::endl;
        delete[] jpegBuffer;
        return false;
    }

    // Escrever o arquivo JPEG no disco
    std::ofstream outputFile(filePath, std::ios::binary);
    if (outputFile) {
        outputFile.write(reinterpret_cast<char*>(jpegBuffer), jpegSize);
        outputFile.close();
        std::cout << "Imagem JPEG salva em: " << filePath << std::endl;
    } else {
        std::cerr << "Erro ao salvar o arquivo JPEG." << std::endl;
        delete[] jpegBuffer;
        return false;
    }

    delete[] jpegBuffer;
    return true;
}

bool salvarStreamEmArquivo(LONG playHandle, char* filePath) {
    std::cout << "Salvando stream de vídeo para o arquivo: " << filePath << std::endl;

    if (!NET_DVR_SaveRealData(playHandle, filePath)) {
        std::cerr << "Erro ao salvar o stream de vídeo: " << NET_DVR_GetLastError() << std::endl;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::seconds(10)); // Grava por 10 segundos

    if (!NET_DVR_StopSaveRealData(playHandle)) {
        std::cerr << "Erro ao parar a gravação do stream de vídeo: " << NET_DVR_GetLastError() << std::endl;
        return false;
    }

    std::cout << "Stream de vídeo salvo com sucesso!" << std::endl;
    return true;
}

#ifdef TESTE_TEMPO_CAPTURA
// Função para calcular FPS com base nos tempos de captura
void calculate_fps(const std::vector<double>& capture_times) {
    double min_fps = 1000.0 / *std::max_element(capture_times.begin(), capture_times.end());
    double max_fps = 1000.0 / *std::min_element(capture_times.begin(), capture_times.end());
    double avg_fps = 1000.0 / (std::accumulate(capture_times.begin(), capture_times.end(), 0.0) / capture_times.size());

    std::cout << "Taxa de FPS máxima: " << max_fps << " FPS\n";
    std::cout << "Taxa de FPS mínima: " << min_fps << " FPS\n";
    std::cout << "Taxa de FPS média: " << avg_fps << " FPS\n\n";
}

void calculate_response_times(const std::vector<double>& capture_times) {
    double min_time = *std::min_element(capture_times.begin(), capture_times.end());
    double max_time = *std::max_element(capture_times.begin(), capture_times.end());
    double avg_time = std::accumulate(capture_times.begin(), capture_times.end(), 0.0) / capture_times.size();

    std::cout << "Tempo de resposta mínimo: " << min_time << " ms\n";
    std::cout << "Tempo de resposta máximo: " << max_time << " ms\n";
    std::cout << "Tempo de resposta médio: " << avg_time << " ms\n\n";
}

#endif

// Função para capturar imagem com o tempo de execução medido
bool captureImage_V50(LONG userID, const char* filePath) {
    NET_DVR_PICPARAM_V50 picParams = {0};

    // Configurando os parâmetros JPEG
    picParams.struParam.wPicSize = 0xff;        // Resolução máxima
    picParams.struParam.wPicQuality = 2;        // Qualidade normal

    // Configurações adicionais
    picParams.byPicFormat = 0;                  // JPEG
    picParams.byCapturePicType = 0;             // Captura geral
    picParams.bySceneID = 0;                    // ID da cena (0 se não suportado)

    // Buffer para armazenar a imagem capturada
    const DWORD bufferSize = 10 * 1024 * 1024;  // Buffer de 10 MB (ajuste conforme necessário)
    char* buffer = new char[bufferSize];
    DWORD sizeReturned = 0;

    std::cout << "Tentando capturar imagem com NET_DVR_CapturePicture_V50()..." << "\n" << std::endl;

    #ifdef TESTE_TEMPO_CAPTURA
        std::vector<double> capture_times;

        for (int i = 0; i < NUM_ITERACOES; ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            // Captura a imagem da câmera e salva no buffer
            if (!NET_DVR_CapturePicture_V50(userID, 1, &picParams, buffer, bufferSize, &sizeReturned)) {
                std::cerr << "Erro ao capturar a imagem (V50): " << NET_DVR_GetLastError() << "\n" << std::endl;
                delete[] buffer;
                return false;
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            // Guardar o tempo de captura
            capture_times.push_back(static_cast<double>(duration));
        }

        // Calcular e exibir o FPS
        calculate_fps(capture_times);
        calculate_response_times(capture_times);

    #else
        // Captura a imagem da câmera e salva no buffer
        if (!NET_DVR_CapturePicture_V50(userID, 1, &picParams, buffer, bufferSize, &sizeReturned)) {
            std::cerr << "Erro ao capturar a imagem (V50): " << NET_DVR_GetLastError() << "\n" << std::endl;
            delete[] buffer;
            return false;
        }
    #endif

    std::cout << "Imagem capturada com sucesso! Tamanho da imagem: " << sizeReturned << " bytes." << std::endl;

    // Salva o buffer em um arquivo
    std::ofstream outputFile(filePath, std::ios::binary);
    if (outputFile) {
        outputFile.write(buffer, sizeReturned);
        outputFile.close();
        std::cout << "Imagem salva em: " << filePath << std::endl;
    } else {
        std::cerr << "Erro ao salvar a imagem no arquivo." << std::endl;
        delete[] buffer;
        return false;
    }

    delete[] buffer;
    return true;
}

 
int main() {
    // Variáveis para IP, porta, login e senha
    const char* ip = ""; // 44 46 64
    WORD port = 8000;
    const char* username = "";
    const char* password = "";
    
    if (!initSDK()) return -1;
    setConnectionOptions();
    
    LONG userID = loginToCamera(ip, port, username, password);

    if (userID >= 0) {
        captureImage_V50(userID, "media/captured_image_v50.jpg");
        /*
        LONG nPort = -1;
        LONG playHandle = startVideoStream(userID, nPort);

        if (playHandle >= 0) {
            
            // 1 get PlayM4GetJPG 
            //captureJPEG(nPort, "img/captured_image_playm4.jpg");
            //PlayM4_Stop(nPort);
            //PlayM4_CloseStream(nPort);

            // 2 get videostream 
            //salvarStreamEmArquivo(playHandle, (char*)"video_output.mp4");
            //NET_DVR_StopRealPlay(playHandle);
        }
        */
        NET_DVR_Logout(userID);
    }

    NET_DVR_Cleanup();
    return 0;
}

