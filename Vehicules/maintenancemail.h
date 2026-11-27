#pragma once

#include <QAbstractSocket>
#include <QDialog>
#include <QList>
#include <QPointer>
#include <QSslError>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSslSocket;
class QTimer;
class Vehicule;

// Réglages enregistrés dans QSettings (aucune adresse ni identifiant dans le code).
struct ReglagesMaintenanceMail
{
    QString serveur = "smtp.gmail.com";
    int port = 465;
    QString utilisateur;
    QString motDePasse;
    QString expediteur;
    QString destinataire;
    bool automatique = true;

    static ReglagesMaintenanceMail charger();
    bool estConfigure() const;
    bool enregistrer() const;
};

// Petit client SMTP asynchrone : SSL implicite (465) ou STARTTLS (587).
class MaintenanceMailClient : public QObject
{
    Q_OBJECT

public:
    MaintenanceMailClient(const ReglagesMaintenanceMail &reglages,
                          const QString &sujet, const QString &corps,
                          QObject *parent = nullptr);
    ~MaintenanceMailClient();

    bool envoyer();

signals:
    void succes();
    void echec(const QString &message);

private slots:
    void surConnexion();
    void surChiffrement();
    void surDeconnexion();
    void surDonnees();
    void surDelai();

private:
    enum Etat {
        AttenteConnexion,
        AttenteGreeting,
        AttenteEhlo,
        AttenteStartTls,
        AttenteChiffrement,
        AttenteAuthLogin,
        AttenteAuthPassword,
        AttenteAuthentification,
        AttenteMailFrom,
        AttenteRcptTo,
        AttenteData,
        AttenteFinData,
        AttenteQuit,
        Termine
    };

    void envoyerCommande(const QByteArray &commande);
    void traiterReponse(int code, const QString &texte);
    void surErreurSocket(QAbstractSocket::SocketError erreur);
    void surErreursSsl(const QList<QSslError> &erreurs);
    void terminer(const QString &message = QString());
    QByteArray construireMessage() const;
    QString erreurServeur(const QString &action, int code, const QString &texte) const;

    ReglagesMaintenanceMail m_reglages;
    QString m_sujet;
    QString m_corps;
    QSslSocket *m_socket;
    QTimer *m_minuterie;
    QByteArray m_message;
    QByteArray m_tampon;
    QString m_texteReponse;
    QString m_capacites;
    Etat m_etat;
    bool m_chiffre;
    bool m_accepte;
    bool m_termine;
    bool m_occupeEnvoi;

    static bool s_envoiEnCours;
};

// Boîte de dialogue créée en code, sans modifier le fichier .ui.
class MaintenanceMailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaintenanceMailDialog(QWidget *parent = nullptr);
    ~MaintenanceMailDialog();

private slots:
    void tester();
    void enregistrer();
    void resultatEnvoi(bool succes, const QString &message);

private:
    ReglagesMaintenanceMail reglagesSaisis() const;
    bool validerReglages(ReglagesMaintenanceMail &reglages, QString &erreur) const;
    void afficherStatut(const QString &message, const QString &couleur);

    QLineEdit *m_serveur;
    QSpinBox *m_port;
    QLineEdit *m_utilisateur;
    QLineEdit *m_motDePasse;
    QLineEdit *m_expediteur;
    QLineEdit *m_destinataire;
    QCheckBox *m_automatique;
    QLabel *m_statut;
    QPushButton *m_boutonTest;
    QPointer<MaintenanceMailClient> m_client;
};

bool adresseMaintenanceMailValide(const QString &adresse);
QString cleNotificationMaintenance(const Vehicule &vehicule);
QString sujetAlerteMaintenance(int nombre);
QString corpsAlerteMaintenance(const QList<Vehicule> &vehicules);
