#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <string.h>

#define MAX_PROCESOS 10
#define MAX_NODOS 10
#define SOLICITUD 1
#define CONFIRMACION 2

typedef mssg_ticket {
    int mtype;  //1=SOLICITUD   2=CONFIRMACION
    int id_nodo_origen;
    int ticket_origen;
    int prioridad_origen;
    int flag_consulta;
};

int confirmaciones = 0;
int num_nodos;
int num_procesos;
int estoy_SC = 0;
int max_ticket = 0;
int mi_ticket;
int mi_prioridad = 0;
int quiero = 0;
int SC_consultas;
int vector_peticiones[MAX_PROCESOS];
sem_t semaforos_de_paso[MAX_PROCESOS];
int flag_pedir_otra_vez[MAX_PROCESOS] = {0};
int nodos_pend = 0;
int id_nodos_pend[MAX_NODOS];
int ticket_nodos_pend[MAX_NODOS];

void *receiver(void *arg) {

    int *msqid = (int *)arg;
    printf("id:%d\n",*msqid);
    struct mssg_ticket mensaje;
    size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype

    while (1) {
        if(msgrcv(*msqid, (void *)&mensaje, buf_length, 0, 0) < 0){
            printf("Error con msgrcv: %s\n", strerror(errno));
            exit(-1);
        }
        max_ticket = MAX(max_ticket, mensaje.ticket_origen);

        if (mensaje.mtype == CONFIRMACION && mensaje.ticket_origen == mi_ticket) {
            confirmaciones++;
            if (confirmaciones == num_nodos) {
                mi_prioridad = 0;
                confirmaciones = 0;
                dar_SC(mensaje.ticket_origen);
            }
        } else if (mensaje.tipo_origen == PETICION && !quiero) {
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        } else if (mensaje.tipo_origen == PETICION && !estoy_SC) {
            if (mensaje.prioridad_origen > mi_prioridad) {
                // Aquí se implementa el envío de confirmación para una peticion mas prioritaria
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                for(int i=0;i<MAX_PROCESOS;i++){
                    if(vector_peticiones[i]==mi_ticket){
                        flag_pedir_otra_vez[i] = 1;
                        sem_post(semaforos_de_paso[i]); //lo despertamos pero debe pedir otra vez a todos
                    }
                }
            } else if (mensaje.prioridad_origen == mi_prioridad && (mensaje.ticket_origen < mi_ticket || (mensaje.ticket_origen == mi_ticket && mensaje.id_nodo_origen < mi_id))) {
                // Aquí se implementa el envío de confirmación para un nodo mas prioritario
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
            } else {
                // Aquí se implementa la lógica de almacenamiento de nodos pendientes
                if (nodos_pend < MAX_NODOS) {
                    id_nodos_pend[nodos_pend] = mensaje.id_nodo_origen;
                    ticket_nodos_pend[nodos_pend] = mensaje.ticket_origen;
                    nodos_pend++;
                } else {
                    // Manejar el caso en que se excede el límite de nodos pendientes
                    printf("Error: Se ha excedido el límite de nodos pendientes.\n");
                }
            }
        } else if (estoy_SC && mensaje.flag_consulta && SC_consultas) {
            // Aquí se implementa el envío de confirmación para una peticion para una Consulta
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        } else {
            // Aquí se implementa la lógica de almacenamiento de nodos pendientes
            if (nodos_pend < MAX_NODOS) {
                id_nodos_pend[nodos_pend] = mensaje.id_nodo_origen;
                ticket_nodos_pend[nodos_pend] = mensaje.ticket_origen;
                nodos_pend++;
            } else {
                // Manejar el caso en que se excede el límite de nodos pendientes
                printf("Error: Se ha excedido el límite de nodos pendientes.\n");
            }
        }
    }
}

void dar_SC(int ticket) {
    for (int i = 0; i < num_procesos; i++) {
        if (vector_peticiones[i] == ticket) {
            flag_pedir_otra_vez[i] = 0;
            sem_post(&semaforos_de_paso[i]); // Damos el paso al que hizo la solicitud
        }
    }
}

void solicitar_SC(int num_proceso, int prioridad_solicitud, int flag_consulta) {
    do {
        quiero = 1;
        if (prioridad_solicitud > mi_prioridad) mi_prioridad = prioridad;
        else while (quiero && !estoy_SC);
        mi_ticket = max_ticket + 1;
        vector_peticiones[num_proceso] = mi_ticket;
        // Preparamos las solicitudes
        struct mssg_ticket solicitud;
        size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
        solicitud.mtype = SOLICITUD;
        solicitud.id_nodo_origen = mi_id;
        solicitud.ticket_origen = mi_ticket;
        solicitud.prioridad_origen = prioridad_solicitud;
        solicitud.flag_consulta = flag_consulta;
        for (int i = 0; i < num_nodos; i++) {
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        }
        sem_wait(&semaforos_de_paso[num_proceso]);
    } while (flag_pedir_otra_vez[num_proceso]);
}

void liberar_SC() {
    quiero = 0;
    // Preparamos las confirmaciones
    struct mssg_ticket confirmacion;
    size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
    solicitud.mtype = CONFIRMACION;
    solicitud.id_nodo_origen = mi_id;
    for (int i = 0; i < nodos_pend; i++) {
        solicitud.ticket_origen = ticket_nodos_pend[i];
        if (msgsnd(id_nodos_pend[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
            printf("Error con msgsnd: %s\n", strerror(errno));
    }
    nodos_pend = 0;
}

void *lector(prioridad, nro_proceso, sem_sinc){
    solicitar_SC(prioridad,nro_proceso,1)
}//aqui

void *escritor(prioridad, nro_proceso, sem_sinc){
    solicitar_SC(prioridad,nro_proceso,0)
}//aqui

int main(int argc, char *argv[]){
    if(argc != xd){
        printf("Uso: %s <id_nodo> <xd>\n", argv[0]);
        printf("Primer nodo = 0 y el último = %d",MAX_NODOS);
        return 1;
    }

    mi_id = atoi(argv[1]);

    //incializar las variables para el intercambio de mensajes
    int msqid_nodos[MAX_NODOS];
    if ((msqid_nodos[mi_id] = msgget(1069+mi_id, IPC_CREAT | IPC_EXCL | 0666)) < 0) printf("Error con msgget: %s\n", strerror(errno));
    printf("Pulsa ENTER cuando todos los nodos estén inicializados\n", nodo);
    while(!getchar());
    for(int i=0;i<MAX_NODOS;i++){
        if(i==mi_id)continue;
        msqid_nodos[i] = msgget(1069+i, 0666); // Colas de mensajes de los nodos
    }

    //inicializar semaforos para sincronizar procesos servidor
    sem_t sem_solicitar_SC[MAX_PROCESOS];
    for(int i=0;i<MAX_PROCESOS;i++){
        if(sem_init(&sem_solicitar_SC[i],0,0)==-1)
            printf("Error con semáforo de paso inicial lector: %s\n", strerror(errno));
    }

    //inicializar los procesos servidores
    pthread_t hilos_servidores[MAX_PROCESOS];
    int opcion, proceso=0;
    do {
        sleep(1);
        /******* Menú de opciones *******/
        printf("\n\n\t\t\tMenu de Procesos\n");
        printf("\t\t\t----------------\n");
        printf("\t[1] Consultas\n");
        printf("\t[2] Reservas\n");
        printf("\t[3] Pagos\n");
        printf("\t[4] Administración\n");
        printf("\t[5] Anulaciones\n");

        printf("\n\tIngrese una opcion: ");
        scanf(" %d", &opcion);

        switch (opcion) {
            case 1: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, lector, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Consulta creado",proceso);
                break;
            case 2: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Reserva creado",proceso);
                break;
            case 3: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Pago creado",proceso);
                break;
            case 4: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Administración creado",proceso);
                break;
            case 5: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Anulaciones creado",proceso);
                break;
            default:
                printf("Error con la elección");
            
        }proceso++;
    }while (proceso<10);

    //inicializamos el receiver
    pthread_t hilo_receptor;
    if ((pthread_create(&hilo_receptor, NULL, receiver, msqid_nodos[mi_id])) < 0)
        printf("Error con pthread_create: %s\n", strerror(errno));
    else printf("Receiver creado",proceso);

    while (1) {
        sleep(1);

        printf ("\n");
        printf ("1. Introducir un proceso en la SC.\n");
        //printf ("2. Quitar un proceso de la SC.\n");
        printf ("3. Salir del programa.\n");
        printf ("Elige una opcion: ");

        scanf ("%i", &opcion);

        switch (opcion) {

            case 1:
                printf ("Que hilo quire introducir en la Seccion Critica? ");
                scanf ("%i", &opcion);
                sem_post (&sem_solicitar_SC[opcion]);
                break;
            case 2:
                printf ("Que hilo quire sacar de la Seccion Critica? ");
                scanf ("%i", &opcion);
                //sem_post (&semaforoSincronizacion[opcion + nHilos]);
                break;
            case 3:
                printf ("Cerrando programa.\n\n");
                if(msgctl(miID, IPC_RMID, NULL) < 0){
                    printf("Error con msgctl: %s\n", strerror(errno));
                    exit(1);
                }else return 0;
            default:
                printf ("Opcion no contemplada.\n");
                break;
        }
    }
}
