#include "maintenancemail.h"
#include "gsmartfirestation.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSslSocket>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QSettings reglagesStockes()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                    "SmartFireStation", "VehiculesMaintenanceMail");
}

QString sansEspaces(const QString &texte)
{
    QString resultat = texte;
    for (int i = resultat.size() - 1; i >= 0; --i) {
        if (resultat.at(i).isSpace())
            resultat.remove(i, 1);
    }
    return resultat;
}
}

ReglagesMaintenanceMail ReglagesMaintenanceMail::charger()
{
    QSettings reglages = reglagesStockes();
    ReglagesMaintenanceMail r;
    r.serveur = reglages.value("serveur", "smtp.gmail.com").toString().trimmed();
    r.port = reglages.value("port", 465).toInt();
    r.utilisateur = reglages.value("utilisateur").toString().trimmed();
    r.motDePasse = sansEspaces(reglages.value("motDePasse").toString());
    r.expediteur = reglages.value("expediteur").toString().trimmed();
    r.destinataire = reglages.value("destinataire").toString().trimmed();
    r.automatique = reglages.value("automatique", true).toBool();
    return r;
}

bool adresseMaintenanceMailValide(const QString &adresse)
{
    if (adresse.isEmpty())
        return false;

    // Une seule adresse simple, sans caractère pouvant injecter un en-tête.
    for (int i = 0; i < adresse.size(); ++i) {
        const QChar c = adresse.at(i);
        if (c == '\r' || c == '\n' || c == '<' || c == '>' || c.isSpace())
            return false;
    }
    if (adresse.contains(',') || adresse.contains(';'))
        return false;

    const int arobase = adresse.indexOf('@');
    return arobase > 0 && arobase == adresse.lastIndexOf('@')
        && arobase < adresse.size() - 1;
}

bool ReglagesMaintenanceMail::estConfigure() const
{
    if (serveur.trimmed().isEmpty() || port < 1 || port > 65535)
        return false;
    for (int i = 0; i < serveur.size(); ++i) {
        if (serveur.at(i).isSpace())
            return false;
    }
    if (!adresseMaintenanceMailValide(utilisateur) || motDePasse.isEmpty())
        return false;
    if (!adresseMaintenanceMailValide(destinataire))
        return false;
    const QString de = expediteur.isEmpty() ? utilisateur : expediteur;
    return adresseMaintenanceMailValide(de);
}

bool ReglagesMaintenanceMail::enregistrer() const
{
    QSettings reglages = reglagesStockes();
    reglages.setValue("serveur", serveur);
    reglages.setValue("port", port);
    reglages.setValue("utilisateur", utilisateur);
    reglages.setValue("motDePasse", motDePasse);
    reglages.setValue("expediteur", expediteur);
    reglages.setValue("destinataire", destinataire);
    reglages.setValue("automatique", automatique);
    // Un nouvel enregistrement autorise de nouveau les alertes automatiques.
    reglages.remove("echecAutomatique");
    reglages.sync();
    return reglages.status() == QSettings::NoError;
}

bool MaintenanceMailClient::s_envoiEnCours = false;

MaintenanceMailClient::MaintenanceMailClient(const ReglagesMaintenanceMail &reglages,
                                             const QString &sujet, const QString &corps,
                                             QObject *parent)
    : QObject(parent), m_reglages(reglages), m_sujet(sujet), m_corps(corps),
      m_socket(nullptr), m_minuterie(nullptr), m_etat(Termine),
      m_chiffre(false), m_accepte(false), m_termine(false), m_occupeEnvoi(false)
{
}

MaintenanceMailClient::~MaintenanceMailClient()
{
    if (m_occupeEnvoi)
        s_envoiEnCours = false;
}

bool MaintenanceMailClient::envoyer()
{
    if (m_termine || s_envoiEnCours)
        return false;
    if (!m_reglages.estConfigure()) {
        emit echec("Les réglages e-mail sont incomplets ou invalides.");
        return false;
    }

    m_socket = new QSslSocket(this);
    m_minuterie = new QTimer(this);
    m_minuterie->setSingleShot(true);
    m_message = construireMessage();
    m_etat = AttenteConnexion;
    m_occupeEnvoi = true;
    s_envoiEnCours = true;

    connect(m_socket, &QAbstractSocket::connected, this, &MaintenanceMailClient::surConnexion);
    connect(m_socket, &QSslSocket::encrypted, this, &MaintenanceMailClient::surChiffrement);
    connect(m_socket, &QAbstractSocket::disconnected, this, &MaintenanceMailClient::surDeconnexion);
    connect(m_socket, &QAbstractSocket::readyRead, this, &MaintenanceMailClient::surDonnees);
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError erreur) { surErreurSocket(erreur); });
    connect(m_socket, &QSslSocket::sslErrors, this,
            [this](const QList<QSslError> &erreurs) { surErreursSsl(erreurs); });
    connect(m_minuterie, &QTimer::timeout, this, &MaintenanceMailClient::surDelai);

    m_minuterie->start(30000);
    if (m_reglages.port == 465)
        m_socket->connectToHostEncrypted(m_reglages.serveur, quint16(m_reglages.port));
    else
        m_socket->connectToHost(m_reglages.serveur, quint16(m_reglages.port));
    return true;
}

void MaintenanceMailClient::envoyerCommande(const QByteArray &commande)
{
    if (m_socket->write(commande) < 0)
        terminer("Impossible d'écrire la commande sur la connexion SMTP.");
}

void MaintenanceMailClient::surConnexion()
{
    // En port 465, on attend la fin du handshake SSL avant de lire le greeting.
    if (m_reglages.port != 465)
        m_etat = AttenteGreeting;
}

void MaintenanceMailClient::surChiffrement()
{
    m_chiffre = true;
    if (m_etat == AttenteChiffrement) {
        m_etat = AttenteEhlo;
        envoyerCommande("EHLO SmartFireStation\r\n");
    } else {
        m_etat = AttenteGreeting;
    }
}

void MaintenanceMailClient::surDeconnexion()
{
    if (m_termine)
        return;
    if (m_accepte)
        terminer();
    else
        terminer("Le serveur SMTP a fermé la connexion avant d'avoir accepté le message.");
}

void MaintenanceMailClient::surErreurSocket(QAbstractSocket::SocketError erreur)
{
    if (m_termine)
        return;
    if (m_accepte) {
        terminer();
        return;
    }

    QString message;
    switch (erreur) {
    case QAbstractSocket::HostNotFoundError:
        message = "Serveur SMTP introuvable. Vérifiez le nom du serveur.";
        break;
    case QAbstractSocket::ConnectionRefusedError:
        message = "Connexion au serveur SMTP refusée. Vérifiez le serveur et le port.";
        break;
    case QAbstractSocket::SslHandshakeFailedError:
        message = "Échec de la connexion chiffrée TLS/SSL avec le serveur SMTP.";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        message = "Le serveur SMTP a fermé la connexion avant la fin de l'envoi.";
        break;
    default:
        message = "Erreur de connexion SMTP : " + m_socket->errorString();
        break;
    }
    terminer(message);
}

void MaintenanceMailClient::surErreursSsl(const QList<QSslError> &erreurs)
{
    if (m_termine)
        return;
    QString detail = erreurs.isEmpty() ? "certificat invalide" : erreurs.first().errorString();
    terminer("Connexion TLS refusée : " + detail);
}

void MaintenanceMailClient::surDonnees()
{
    if (m_termine)
        return;

    // Le délai court de nouveau dès que le serveur envoie une donnée.
    m_minuterie->start(30000);
    m_tampon += m_socket->readAll();

    while (m_tampon.contains("\r\n")) {
        const int fin = m_tampon.indexOf("\r\n");
        const QString ligne = QString::fromUtf8(m_tampon.left(fin));
        m_tampon.remove(0, fin + 2);
        if (ligne.size() < 3)
            continue;

        const int code = ligne.left(3).toInt();
        const QString texte = ligne.size() > 3 ? ligne.mid(4) : QString();
        if (ligne.size() > 3 && ligne.at(3) == '-') {
            if (!m_texteReponse.isEmpty())
                m_texteReponse += "\n";
            m_texteReponse += texte;
            continue;
        }

        if (!m_texteReponse.isEmpty())
            m_texteReponse += "\n";
        m_texteReponse += texte;
        traiterReponse(code, m_texteReponse);
        m_texteReponse.clear();
        if (m_termine)
            break;
    }
}

void MaintenanceMailClient::surDelai()
{
    if (m_termine)
        return;
    if (m_accepte)
        terminer(); // le message est déjà accepté, QUIT n'est plus indispensable
    else
        terminer("Délai dépassé : aucune réponse du serveur SMTP depuis 30 secondes.");
}

QString MaintenanceMailClient::erreurServeur(const QString &action, int code,
                                             const QString &texte) const
{
    QString detail = texte.trimmed();
    if (detail.isEmpty())
        detail = "réponse du serveur";
    QString message = QString("%1 (erreur SMTP %2 : %3)").arg(action).arg(code).arg(detail);
    if (code == 535)
        message += "\nGmail : utilisez un mot de passe d'application.";
    return message;
}

void MaintenanceMailClient::traiterReponse(int code, const QString &texte)
{
    switch (m_etat) {
    case AttenteGreeting:
        if (code != 220) {
            terminer(erreurServeur("Le serveur SMTP a refusé la connexion", code, texte));
            return;
        }
        m_etat = AttenteEhlo;
        envoyerCommande("EHLO SmartFireStation\r\n");
        break;

    case AttenteEhlo:
        if (code != 250) {
            terminer(erreurServeur("La commande EHLO a été refusée", code, texte));
            return;
        }
        m_capacites = texte.toUpper();
        if (!m_chiffre) {
            if (!m_capacites.contains("STARTTLS")) {
                terminer("Connexion non chiffrée : le serveur n'accepte pas STARTTLS. "
                         "Les identifiants ne seront pas envoyés.");
                return;
            }
            m_etat = AttenteStartTls;
            envoyerCommande("STARTTLS\r\n");
        } else {
            m_etat = AttenteAuthLogin;
            envoyerCommande("AUTH LOGIN\r\n");
        }
        break;

    case AttenteStartTls:
        if (code != 220) {
            terminer(erreurServeur("Le serveur a refusé STARTTLS", code, texte));
            return;
        }
        m_etat = AttenteChiffrement;
        m_socket->startClientEncryption();
        break;

    case AttenteAuthLogin:
        if (code != 334) {
            terminer(erreurServeur("L'authentification SMTP a été refusée", code, texte));
            return;
        }
        m_etat = AttenteAuthPassword;
        envoyerCommande(m_reglages.utilisateur.toUtf8().toBase64() + "\r\n");
        break;

    case AttenteAuthPassword:
        if (code != 334) {
            terminer(erreurServeur("L'authentification SMTP a été refusée", code, texte));
            return;
        }
        m_etat = AttenteAuthentification;
        envoyerCommande(m_reglages.motDePasse.toUtf8().toBase64() + "\r\n");
        break;

    case AttenteAuthentification:
        if (code != 235) {
            terminer(erreurServeur("Identifiants SMTP incorrects", code, texte));
            return;
        }
        m_etat = AttenteMailFrom;
        {
            const QByteArray de = m_reglages.expediteur.isEmpty()
                ? m_reglages.utilisateur.toUtf8() : m_reglages.expediteur.toUtf8();
            envoyerCommande("MAIL FROM:<" + de + ">\r\n");
        }
        break;

    case AttenteMailFrom:
        if (code != 250) {
            terminer(erreurServeur("L'adresse d'expéditeur a été refusée", code, texte));
            return;
        }
        m_etat = AttenteRcptTo;
        envoyerCommande("RCPT TO:<" + m_reglages.destinataire.toUtf8() + ">\r\n");
        break;

    case AttenteRcptTo:
        if (code != 250 && code != 251) {
            terminer(erreurServeur("L'adresse du destinataire a été refusée", code, texte));
            return;
        }
        m_etat = AttenteData;
        envoyerCommande("DATA\r\n");
        break;

    case AttenteData:
        if (code != 354) {
            terminer(erreurServeur("Le serveur a refusé le contenu du message", code, texte));
            return;
        }
        m_etat = AttenteFinData;
        {
            QByteArray donnees = m_message;
            donnees.replace("\r\n.", "\r\n..");
            if (donnees.startsWith('.'))
                donnees.prepend('.');
            envoyerCommande(donnees + "\r\n.\r\n");
        }
        break;

    case AttenteFinData:
        if (code != 250) {
            terminer(erreurServeur("Le serveur n'a pas accepté le message", code, texte));
            return;
        }
        m_accepte = true;
        emit succes();
        m_etat = AttenteQuit;
        envoyerCommande("QUIT\r\n");
        break;

    case AttenteQuit:
        terminer();
        break;

    default:
        terminer("État SMTP inattendu : arrêt de l'envoi.");
        break;
    }
}

void MaintenanceMailClient::terminer(const QString &message)
{
    if (m_termine)
        return;
    m_termine = true;
    m_etat = Termine;
    m_minuterie->stop();
    if (m_occupeEnvoi) {
        m_occupeEnvoi = false;
        s_envoiEnCours = false;
    }
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
    if (!message.isEmpty() && !m_accepte)
        emit echec(message);
    deleteLater();
}

QByteArray MaintenanceMailClient::construireMessage() const
{
    const QString de = m_reglages.expediteur.isEmpty() ? m_reglages.utilisateur
                                                      : m_reglages.expediteur;
    const QString domaine = de.section('@', 1, 1);
    const QByteArray identifiant = QByteArray::number(QDateTime::currentMSecsSinceEpoch())
        + "." + QByteArray::number(quint64(qHash(m_reglages.utilisateur)), 16)
        + "@" + domaine.toUtf8();
    const QByteArray sujet = "=?UTF-8?B?" + m_sujet.toUtf8().toBase64() + "?=";

    QByteArray message;
    message += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    message += "From: " + de.toUtf8() + "\r\n";
    message += "To: " + m_reglages.destinataire.toUtf8() + "\r\n";
    message += "Message-ID: <" + identifiant + ">\r\n";
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: text/plain; charset=UTF-8\r\n";
    message += "Content-Transfer-Encoding: base64\r\n";
    message += "Subject: " + sujet + "\r\n\r\n";

    const QByteArray corps = m_corps.toUtf8().toBase64();
    for (int i = 0; i < corps.size(); i += 76) {
        message += corps.mid(i, 76);
        message += "\r\n";
    }
    return message;
}

MaintenanceMailDialog::MaintenanceMailDialog(QWidget *parent)
    : QDialog(parent), m_serveur(nullptr), m_port(nullptr), m_utilisateur(nullptr),
      m_motDePasse(nullptr), m_expediteur(nullptr), m_destinataire(nullptr),
      m_automatique(nullptr), m_statut(nullptr), m_boutonTest(nullptr)
{
    setWindowTitle("Alertes e-mail de maintenance");
    setMinimumWidth(540);

    const ReglagesMaintenanceMail r = ReglagesMaintenanceMail::charger();
    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->setContentsMargins(22, 18, 22, 18);
    principal->setSpacing(10);

    QLabel *titre = new QLabel("Alertes e-mail de maintenance");
    titre->setStyleSheet("font-size:18px; font-weight:bold; color:#1F4E79;");
    principal->addWidget(titre);

    QFormLayout *formulaire = new QFormLayout;
    formulaire->setSpacing(9);
    m_serveur = new QLineEdit(r.serveur);
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(r.port);
    m_utilisateur = new QLineEdit(r.utilisateur);
    m_motDePasse = new QLineEdit(r.motDePasse);
    m_motDePasse->setEchoMode(QLineEdit::Password);
    m_expediteur = new QLineEdit(r.expediteur);
    m_destinataire = new QLineEdit(r.destinataire);
    m_automatique = new QCheckBox("Envoyer automatiquement l'alerte dès qu'une maintenance est dépassée");
    m_automatique->setChecked(r.automatique);

    m_serveur->setPlaceholderText("smtp.gmail.com");
    m_utilisateur->setPlaceholderText("Votre adresse Gmail");
    m_motDePasse->setPlaceholderText("Mot de passe d'application Gmail");
    m_expediteur->setPlaceholderText("Vide = même adresse Gmail");
    m_destinataire->setPlaceholderText("Une seule adresse e-mail");

    formulaire->addRow("Serveur SMTP :", m_serveur);
    formulaire->addRow("Port :", m_port);
    formulaire->addRow("Adresse Gmail :", m_utilisateur);
    formulaire->addRow("Mot de passe :", m_motDePasse);
    formulaire->addRow("Expéditeur :", m_expediteur);
    formulaire->addRow("Destinataire :", m_destinataire);
    principal->addLayout(formulaire);
    principal->addWidget(m_automatique);

    m_statut = new QLabel("Aucun e-mail envoyé.");
    m_statut->setWordWrap(true);
    m_statut->setMinimumHeight(24);
    afficherStatut("Aucun e-mail envoyé.", "#6B7280");
    principal->addWidget(m_statut);

    m_boutonTest = new QPushButton("Envoyer un e-mail de test");
    m_boutonTest->setProperty("class", "btnPrimary");
    QPushButton *boutonEnregistrer = new QPushButton("Enregistrer");
    boutonEnregistrer->setProperty("class", "btnSuccess");
    QPushButton *boutonAnnuler = new QPushButton("Annuler");
    boutonAnnuler->setProperty("class", "btnSecondary");

    QHBoxLayout *boutons = new QHBoxLayout;
    boutons->addWidget(m_boutonTest);
    boutons->addStretch();
    boutons->addWidget(boutonEnregistrer);
    boutons->addWidget(boutonAnnuler);
    principal->addLayout(boutons);

    connect(m_boutonTest, &QPushButton::clicked, this, &MaintenanceMailDialog::tester);
    connect(boutonEnregistrer, &QPushButton::clicked, this, &MaintenanceMailDialog::enregistrer);
    connect(boutonAnnuler, &QPushButton::clicked, this, &QDialog::reject);
    repolish(this);
}

MaintenanceMailDialog::~MaintenanceMailDialog()
{
    if (m_client) {
        m_client->disconnect(this);
        delete m_client;
        m_client = nullptr;
    }
}

ReglagesMaintenanceMail MaintenanceMailDialog::reglagesSaisis() const
{
    ReglagesMaintenanceMail r;
    r.serveur = m_serveur->text().trimmed();
    r.port = m_port->value();
    r.utilisateur = m_utilisateur->text().trimmed();
    r.motDePasse = sansEspaces(m_motDePasse->text());
    r.expediteur = m_expediteur->text().trimmed();
    r.destinataire = m_destinataire->text().trimmed();
    r.automatique = m_automatique->isChecked();
    return r;
}

bool MaintenanceMailDialog::validerReglages(ReglagesMaintenanceMail &r, QString &erreur) const
{
    if (r.serveur.isEmpty()) {
        erreur = "Le serveur SMTP est obligatoire.";
        return false;
    }
    for (int i = 0; i < r.serveur.size(); ++i) {
        if (r.serveur.at(i).isSpace() || r.serveur.at(i) == '\r' || r.serveur.at(i) == '\n') {
            erreur = "Le serveur SMTP ne doit pas contenir d'espace.";
            return false;
        }
    }
    if (r.port < 1 || r.port > 65535) {
        erreur = "Le port SMTP doit être compris entre 1 et 65535.";
        return false;
    }
    if (!adresseMaintenanceMailValide(r.utilisateur)) {
        erreur = "L'adresse Gmail est invalide.";
        return false;
    }
    if (r.motDePasse.isEmpty()) {
        erreur = "Le mot de passe est obligatoire.";
        return false;
    }
    if (r.expediteur.isEmpty())
        r.expediteur = r.utilisateur;
    if (!adresseMaintenanceMailValide(r.expediteur)) {
        erreur = "L'adresse de l'expéditeur est invalide.";
        return false;
    }
    if (!adresseMaintenanceMailValide(r.destinataire)) {
        erreur = "Le destinataire doit être une seule adresse e-mail valide.";
        return false;
    }
    return true;
}

void MaintenanceMailDialog::afficherStatut(const QString &message, const QString &couleur)
{
    m_statut->setText(message);
    m_statut->setStyleSheet(QString("color:%1;").arg(couleur));
}

void MaintenanceMailDialog::tester()
{
    if (m_client)
        return;

    ReglagesMaintenanceMail r = reglagesSaisis();
    QString erreur;
    if (!validerReglages(r, erreur)) {
        afficherStatut(erreur, "#C00000");
        return;
    }

    m_client = new MaintenanceMailClient(r, "Smart Fire Station - E-mail de test",
        "Bonjour,\n\nLa configuration des alertes e-mail de maintenance fonctionne.\n\n"
        "— Smart Fire Station", this);
    connect(m_client, &MaintenanceMailClient::succes, this,
            [this]() { resultatEnvoi(true, "E-mail de test envoyé avec succès."); });
    connect(m_client, &MaintenanceMailClient::echec, this,
            [this](const QString &message) { resultatEnvoi(false, message); });
    connect(m_client, &QObject::destroyed, this, [this]() {
        m_client = nullptr;
        m_boutonTest->setEnabled(true);
    });

    m_boutonTest->setEnabled(false);
    afficherStatut("Envoi de l'e-mail de test en cours…", "#1F4E79");
    if (!m_client->envoyer()) {
        resultatEnvoi(false, "Un autre e-mail est déjà en cours d'envoi.");
        if (m_client)
            m_client->deleteLater();
    }
}

void MaintenanceMailDialog::resultatEnvoi(bool succes, const QString &message)
{
    afficherStatut(message, succes ? "#2E7D32" : "#C00000");
}

void MaintenanceMailDialog::enregistrer()
{
    ReglagesMaintenanceMail r = reglagesSaisis();
    QString erreur;
    if (!validerReglages(r, erreur)) {
        afficherStatut(erreur, "#C00000");
        return;
    }
    if (!r.enregistrer()) {
        afficherStatut("Impossible d'enregistrer les réglages e-mail.", "#C00000");
        return;
    }
    accept();
}

QString cleNotificationMaintenance(const Vehicule &vehicule)
{
    return vehicule.id() + vehicule.dateMaintenance().toString("yyyyMMdd");
}

QString sujetAlerteMaintenance(int nombre)
{
    return QString("Smart Fire Station - %1 véhicule(s) en retard de maintenance").arg(nombre);
}

QString corpsAlerteMaintenance(const QList<Vehicule> &vehicules)
{
    QString corps = QString("Bonjour,\n\n"
        "Smart Fire Station signale des véhicules dont la date de maintenance est dépassée (%1) :\n\n")
        .arg(QDate::currentDate().toString("dd/MM/yyyy"));

    for (const Vehicule &vehicule : vehicules) {
        corps += QString::fromUtf8("• %1 — %2 (%3, %4) : maintenance prévue le %5, en retard de %6 jour(s)")
            .arg(vehicule.id()).arg(vehicule.immatriculation())
            .arg(vehicule.type()).arg(vehicule.modele())
            .arg(vehicule.dateMaintenance().toString("dd/MM/yyyy"))
            .arg(-vehicule.joursAvantMaintenance());

        if (vehicule.disponibilite() == "En mission") {
            corps += QString::fromUtf8(" — actuellement en mission : maintenance à effectuer dès la fin de la mission");
        } else if (vehicule.disponibilite() == "En maintenance") {
            corps += QString::fromUtf8(" — retiré du service (en maintenance)");
        } else {
            corps += QString(" — actuellement %1").arg(vehicule.disponibilite().toLower());
        }
        corps += "\n";
    }

    corps += "\nMerci de planifier ces interventions.\n\n"
             "— Smart Fire Station (message automatique)";
    return corps;
}
