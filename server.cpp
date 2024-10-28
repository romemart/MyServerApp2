#include "server.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <future>

Server::Server(int port) : port(port), running(true) {
    // Crear el socket del servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        throw std::runtime_error("Error al crear el socket.");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Enlazar el socket a la dirección y puerto
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Error en el bind.");
    }
}

void Server::handle_client(int client_socket) {
    // Crear una promesa y un future asociado
    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    // Leer y enviar datos de forma asincrónica
    std::thread([client_socket, promise = std::move(promise)]() mutable {
        // Crear el buffer dentro del lambda para que viva en el contexto del hilo
        char buffer[1024] = {0};
        
        try {
            while (true) {
                int bytes_read = read(client_socket, buffer, 1024);
                if (bytes_read <= 0) {
                    // El cliente cerró la conexión
                    break;
                }
                
                std::cout << "Mensaje recibido: " << buffer << std::endl;
                
                const char* response = "Mensaje recibido";
                if (send(client_socket, response, strlen(response), 0) < 0) {
                    // Error al enviar la respuesta
                    break;
                }
                
                memset(buffer, 0, sizeof(buffer));
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error en el hilo del cliente: " << e.what() << std::endl;
        }

        // Cerrar el socket y notificar que hemos terminado
        close(client_socket);
        promise.set_value();
    }).detach();

    // Esperar a que el hilo termine
    future.wait();
}

void Server::start() {
    if (listen(server_socket, 5) < 0) {
        throw std::runtime_error("Error en el listen.");
    }
    std::cout << "Servidor escuchando en el puerto " << port << std::endl;

    while (running) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            std::cerr << "Error al aceptar conexión." << std::endl;
            continue;
        }

        // Crear un futuro para manejar la conexión del cliente de manera asincrónica
        std::future<void> client_future = std::async(std::launch::async, &Server::handle_client, this, client_socket);
    }

    close(server_socket);
}

void Server::stop() {
    running = false; // Cambia la bandera para detener el bucle de aceptación de conexiones
    close(server_socket);
    std::cout << "Servidor detenido." << std::endl;
}