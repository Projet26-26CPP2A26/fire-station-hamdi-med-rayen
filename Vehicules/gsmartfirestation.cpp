#include "gsmartfirestation.h"
#include "ui_gsmartfirestation.h"

// ===== databasemanager =====
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlError>

QSqlDatabase DatabaseManager::s_db;
bool DatabaseManager::s_connected = false;

bool DatabaseManager::connectInteractive(QWidget *parent)
{
    // Connexion via ODBC (pilote Windows "Oracle in XE", 64 bits).
    if (QSqlDatabase::isDriverAvailable("QODBC") == false) {
        QMessageBox::warning(parent, "Pilote ODBC manquant",
            "Le pilote Qt QODBC n'est pas disponible.\n"
            "L'application peut s'ouvrir en mode démonstration, mais ne pourra pas lire/écrire dans Oracle.");
        return false;
    }

    bool ok = false;
    QString host = QInputDialog::getText(parent, "Connexion Oracle",
        "Hôte Oracle :", QLineEdit::Normal, "localhost", &ok);
    if (!ok) return false;
    if (host.trimmed().isEmpty()) host = "localhost";

    int port = QInputDialog::getInt(parent, "Connexion Oracle",
        "Port Oracle :", 1521, 1, 65535, 1, &ok);
    if (!ok) return false;

    QString sid = QInputDialog::getText(parent, "Connexion Oracle",
        "SID ou nom de service (ex. XE) :", QLineEdit::Normal, "XE", &ok);
    if (!ok || sid.trimmed().isEmpty()) return false;

    QString user = QInputDialog::getText(parent, "Connexion Oracle",
        "Nom d'utilisateur Oracle :", QLineEdit::Normal, "", &ok);
    if (!ok || user.trimmed().isEmpty()) return false;

    QString password = QInputDialog::getText(parent, "Connexion Oracle",
        "Mot de passe Oracle :", QLineEdit::Password, "", &ok);
    if (!ok) return false;

    const QString connectionName = "SmartFireStationOracle";
    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);

    s_db = QSqlDatabase::addDatabase("QODBC", connectionName);
    // Qt ajoute lui-même UID= et PWD= à partir de setUserName / setPassword.
    s_db.setDatabaseName(QString("DRIVER={Oracle in XE};DBQ=//%1:%2/%3;")
                             .arg(host.trimmed())
                             .arg(port)
                             .arg(sid.trimmed()));
    s_db.setUserName(user.trimmed());
    s_db.setPassword(password);

    if (!s_db.open()) {
        const QString error = s_db.lastError().text();
        s_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        s_connected = false;
        QMessageBox::warning(parent, "Connexion Oracle échouée",
            "Impossible de se connecter à Oracle.\n\nDétail : " + error +
            "\n\nVérifiez l'hôte, le port, le SID/service, l'utilisateur, le mot de passe "
            "et que le service Oracle est démarré.");
        return false;
    }

    s_connected = true;
    QMessageBox::information(parent, "Oracle connecté",
        "Connexion Oracle établie.\nLes données seront chargées depuis la base.");
    return true;
}

bool DatabaseManager::isConnected() { return s_connected && s_db.isOpen(); }
QSqlDatabase DatabaseManager::database() { return s_db; }
void DatabaseManager::close()
{
    if (s_db.isValid()) s_db.close();
    s_connected = false;
}

// ===== pdfexport =====

#include <QDateTime>
#include <QFileInfo>
#include <QPdfWriter>
#include <QTextDocument>

bool creerPdfTableau(const QString &chemin, const QString &titre,
                     const QStringList &entetes, const QList<QStringList> &lignes)
{
    // 1) On écrit le contenu du PDF en HTML (titre + tableau)
    QString html = "<h2 style='color:#1F4E79;'>Smart Fire Station Management</h2>";
    html += "<h3>" + titre + "</h3>";
    html += "<p>Exporté le " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</p>";
    html += "<table border='1' cellspacing='0' cellpadding='5' width='100%'><tr>";
    for (int c = 0; c < entetes.size(); c++)
        html += "<th bgcolor='#1F4E79' style='color:white;'>" + entetes[c] + "</th>";
    html += "</tr>";
    for (int l = 0; l < lignes.size(); l++) {
        html += "<tr>";
        for (int c = 0; c < lignes[l].size(); c++)
            html += "<td>" + lignes[l][c] + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    // 2) On "imprime" ce HTML dans un fichier PDF
    QPdfWriter writer(chemin);
    QTextDocument document;
    document.setHtml(html);
    document.print(&writer);

    return QFileInfo(chemin).size() > 0;
}

// ===== personnel =====

Personnel::Personnel()
{
    m_missionsEnCours = 0;
}

Personnel::Personnel(const QString &id, const QString &nom, const QString &prenom,
                 const QString &grade, const QString &poste, const QString &telephone,
                 const QString &email, const QString &motDePasse,
                 const QString &disponibilite, int missionsEnCours)
{
    m_id = id;
    m_nom = nom;
    m_prenom = prenom;
    m_grade = grade;
    m_poste = poste;
    m_telephone = telephone;
    m_email = email;
    m_motDePasse = motDePasse;
    m_disponibilite = disponibilite;
    m_missionsEnCours = missionsEnCours;
}

int Personnel::charge() const
{
    int c = m_missionsEnCours * 20; // 20 % par mission
    if (c > 100)
        c = 100;
    return c;
}

QString Personnel::nomComplet() const
{
    return m_prenom + " " + m_nom;
}

// Du grade le plus élevé au moins élevé (sert aussi pour le tri)
QStringList Personnel::grades()
{
    QStringList liste;
    liste << "Colonel" << "Commandant" << "Capitaine" << "Lieutenant"
          << "Adjudant" << "Sergent" << "Caporal" << "Sapeur";
    return liste;
}

QStringList Personnel::postes()
{
    QStringList liste;
    liste << "Chef de garde" << "Intervention" << "Secours" << "Logistique" << "Administration" << "Pompier";
    return liste;
}

QStringList Personnel::disponibilites()
{
    QStringList liste;
    liste << "Disponible" << "En mission" << "En congé";
    return liste;
}

int Personnel::rangGrade(const QString &grade)
{
    int i = grades().indexOf(grade);
    if (i < 0)
        i = grades().size();
    return i;
}

// ===== vehicule =====

Vehicule::Vehicule()
{
}

Vehicule::Vehicule(const QString &id, const QString &immatriculation, const QString &type,
                   const QString &modele, const QString &etat, const QString &disponibilite,
                   const QDate &dateMaintenance)
{
    m_id = id;
    m_immatriculation = immatriculation;
    m_type = type;
    m_modele = modele;
    m_etat = etat;
    m_disponibilite = disponibilite;
    m_dateMaintenance = dateMaintenance;
}

bool Vehicule::maintenanceProche() const
{
    QDate limite = QDate::currentDate().addDays(30);
    return m_dateMaintenance.isValid() && m_dateMaintenance <= limite;
}

QStringList Vehicule::types()
{
    QStringList liste;
    liste << "Fourgon" << "Camion" << "Ambulance" << "Citerne" << "Échelle";
    return liste;
}

QStringList Vehicule::etats()
{
    QStringList liste;
    liste << "Bon" << "À contrôler" << "Hors service";
    return liste;
}

QStringList Vehicule::disponibilites()
{
    QStringList liste;
    liste << "Disponible" << "En mission" << "En maintenance";
    return liste;
}

// Types de véhicules adaptés à chaque intervention, par ordre de préférence
QStringList Vehicule::typesPourIntervention(const QString &intervention)
{
    QStringList liste;
    if (intervention == "Incendie")
        liste << "Citerne" << "Fourgon" << "Échelle";
    else if (intervention == "Secours à personne")
        liste << "Ambulance" << "Fourgon";
    else if (intervention == "Accident de la route")
        liste << "Camion" << "Fourgon" << "Ambulance";
    else if (intervention == "Inondation")
        liste << "Camion" << "Citerne";
    else
        liste = types();
    return liste;
}

// ===== piechartwidget =====
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QRectF>
#include <QtMath>

PieChartWidget::PieChartWidget(const QString &centerLabel, QWidget *parent)
    : QWidget(parent), m_centerLabel(centerLabel)
{
    setMinimumHeight(235);
    setMinimumWidth(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}
void PieChartWidget::setData(const QStringList &labels, const QList<int> &values)
{
    m_labels = labels; m_values = values; update();
}
void PieChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor colors[] = {QColor("#1F4E79"), QColor("#DC3C2D"), QColor("#2E7D32"), QColor("#E6A23C"), QColor("#7C3AED"), QColor("#0891B2"), QColor("#64748B"), QColor("#DB2777")};
    int total = 0;
    for (int v : m_values) total += qMax(0, v);
    // Responsive layout: reserve a clear legend area and keep the circle fully visible.
    const int legendWidth = qMin(190, qMax(112, width() * 46 / 100));
    const int availablePie = qMax(90, width() - legendWidth - 24);
    const int diameter = qMax(90, qMin(170, qMin(availablePie - 8, height() - 24)));
    const int px = 8;
    const int py = qMax(8, (height() - diameter) / 2);
    QRectF pie(px, py, diameter, diameter);
    if (total <= 0) {
        p.setPen(QPen(QColor("#E5E7EB"), 18)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(pie.adjusted(10,10,-10,-10));
        p.setPen(QColor("#64748B")); p.drawText(pie, Qt::AlignCenter, "Aucune donnée"); return;
    }
    int start = 90 * 16;
    for (int i=0; i<m_values.size(); ++i) {
        int val = qMax(0,m_values[i]); if (!val) continue;
        int span = -qRound(360.0 * 16 * val / total);
        p.setPen(QPen(Qt::white, 1.5)); p.setBrush(colors[i % 8]); p.drawPie(pie, start, span); start += span;
    }
    int hole = diameter / 3;
    p.setPen(Qt::white); p.setBrush(Qt::white); p.drawEllipse(pie.adjusted(hole,hole,-hole,-hole));
    p.setPen(QColor("#1F2937")); QFont f=p.font(); f.setBold(true); f.setPointSize(17); p.setFont(f);
    p.drawText(QRectF(pie.x()+diameter/4, pie.y()+diameter/2-18, diameter/2, 24), Qt::AlignCenter, QString::number(total));
    f.setBold(false); f.setPointSize(8); p.setFont(f); p.setPen(QColor("#64748B"));
    p.drawText(QRectF(pie.x()+diameter/4, pie.y()+diameter/2+5, diameter/2, 18), Qt::AlignCenter, QStringLiteral("Total"));
    const int legendX = px + diameter + 12;
    const int lineHeight = qMin(28, qMax(20, (height()-16) / qMax(1,m_labels.size())));
    f=p.font(); f.setPointSize(8); p.setFont(f);
    for (int i=0; i<m_labels.size() && i<m_values.size(); ++i) {
        int y = 8 + i * lineHeight;
        p.setPen(Qt::NoPen); p.setBrush(colors[i%8]); p.drawRoundedRect(QRectF(legendX,y+5,9,9),2,2);
        double pct = total ? (100.0*m_values[i]/total) : 0;
        p.setPen(QColor("#334155"));
        QString line = QString("%1  %2 (%3%)").arg(m_labels[i]).arg(m_values[i]).arg(qRound(pct));
        QRectF textRect(legendX+14,y,width()-legendX-17,lineHeight);
        p.drawText(textRect, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap, line);
    }
}

// ===== statsdialog =====

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {
class DonutChart : public QWidget
{
public:
    DonutChart(const QStringList &labels, const QList<int> &values, QWidget *parent = 0)
        : QWidget(parent), m_labels(labels), m_values(values)
    {
        setMinimumSize(280, 230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(250);
    }
protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int total = [&]() { int t=0; for (int v : m_values) t += v; return t; }();
        QRectF rect((width()-190)/2.0, 14, 190, 190);
        const QColor colors[] = {QColor("#2E7D32"), QColor("#DC3C2D"), QColor("#1F4E79"), QColor("#E6A23C"), QColor("#7E57C2"), QColor("#607D8B")};
        if (total <= 0) {
            painter.setPen(QPen(QColor("#E5E7EB"), 26)); painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(rect.adjusted(18,18,-18,-18));
            painter.setPen(QColor("#64748B")); painter.drawText(rect, Qt::AlignCenter, "Aucune donnée");
            return;
        }
        int start = 90 * 16;
        for (int i=0; i<m_values.size(); ++i) {
            int span = -qRound(360.0 * 16 * m_values[i] / total);
            painter.setPen(QPen(Qt::white, 2)); painter.setBrush(colors[i % 6]);
            painter.drawPie(rect, start, span); start += span;
        }
        painter.setPen(Qt::white); painter.setBrush(Qt::white);
        painter.drawEllipse(rect.adjusted(47,47,-47,-47));
        painter.setPen(QColor("#1F2937"));
        QFont f = painter.font(); f.setBold(true); f.setPointSize(20); painter.setFont(f);
        painter.drawText(rect.adjusted(35,55,-35,-55), Qt::AlignCenter, QString::number(total));
        f.setBold(false); f.setPointSize(9); painter.setFont(f); painter.setPen(QColor("#64748B"));
        painter.drawText(rect.adjusted(35,87,-35,-30), Qt::AlignCenter, "TOTAL");
    }
private:
    QStringList m_labels;
    QList<int> m_values;
};
}

StatCritere compterParValeur(const QString &nom, const QStringList &possibles,
                             const QStringList &reelles)
{
    StatCritere resultat; resultat.nom = nom;
    for (int p=0; p<possibles.size(); ++p) {
        int n=0; for (int r=0; r<reelles.size(); ++r) if (reelles[r] == possibles[p]) ++n;
        if (n>0) { resultat.etiquettes << possibles[p]; resultat.valeurs << n; }
    }
    return resultat;
}

StatsDialog::StatsDialog(const QString &titre, const QList<StatCritere> &criteres, QWidget *parent)
    : QDialog(parent)
{
    m_criteres=criteres; setWindowTitle(titre); setMinimumSize(540, 480);
    QLabel *labelTitre=new QLabel(titre); labelTitre->setProperty("class","cardTitle");
    m_combo=new QComboBox;
    for (int i=0;i<m_criteres.size();++i) m_combo->addItem(m_criteres[i].nom);
    QHBoxLayout *ligneCritere=new QHBoxLayout;
    ligneCritere->addWidget(new QLabel("Répartition selon :")); ligneCritere->addWidget(m_combo); ligneCritere->addStretch();
    m_grille=new QGridLayout; m_grille->setVerticalSpacing(10);
    QPushButton *fermer=new QPushButton("Fermer"); fermer->setProperty("class","btnPrimary");
    connect(fermer,SIGNAL(clicked()),this,SLOT(close()));
    QHBoxLayout *ligneBoutons=new QHBoxLayout; ligneBoutons->addStretch(); ligneBoutons->addWidget(fermer);
    QVBoxLayout *principal=new QVBoxLayout(this); principal->setContentsMargins(24,20,24,20); principal->setSpacing(12);
    principal->addWidget(labelTitre); principal->addLayout(ligneCritere); principal->addLayout(m_grille,1); principal->addLayout(ligneBoutons);
    connect(m_combo,SIGNAL(currentIndexChanged(int)),this,SLOT(afficherCritere(int)));
    afficherCritere(0); repolish(this);
}

void StatsDialog::afficherCritere(int index)
{
    if (index<0 || index>=m_criteres.size()) return;
    while (QLayoutItem *item=m_grille->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    const StatCritere c=m_criteres[index]; int total=0; for (int v:c.valeurs) total+=v;
    DonutChart *chart=new DonutChart(c.etiquettes,c.valeurs,this); m_grille->addWidget(chart,0,0,1,3,Qt::AlignHCenter);
    const QColor colors[]={QColor("#2E7D32"),QColor("#DC3C2D"),QColor("#1F4E79"),QColor("#E6A23C"),QColor("#7E57C2"),QColor("#607D8B")};
    for (int i=0;i<c.etiquettes.size();++i) {
        QLabel *swatch=new QLabel("●"); swatch->setStyleSheet(QString("color:%1; font-size:17px;").arg(colors[i%6].name()));
        int pct=total ? qRound(100.0*c.valeurs[i]/total) : 0;
        QLabel *label=new QLabel(c.etiquettes[i]); QLabel *value=new QLabel(QString("%1  •  %2 %").arg(c.valeurs[i]).arg(pct));
        value->setStyleSheet("font-weight:600; color:#1F2937;");
        m_grille->addWidget(swatch,i+1,0); m_grille->addWidget(label,i+1,1); m_grille->addWidget(value,i+1,2,Qt::AlignRight);
    }
    QLabel *totalLabel=new QLabel(QString("Total : %1 personne(s)").arg(total)); totalLabel->setStyleSheet("font-weight:600; color:#1F4E79; padding-top:8px;");
    m_grille->addWidget(totalLabel,c.etiquettes.size()+1,0,1,3);
    m_grille->setColumnStretch(1,1);
}

// ===== personneldialog =====

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PersonnelDialog::PersonnelDialog(const QString &titre, const QString &id, QWidget *parent)
    : QDialog(parent)
{
    m_id = id;
    setWindowTitle(titre);
    setMinimumWidth(420);

    // Champs du formulaire
    m_nom = new QLineEdit;
    m_prenom = new QLineEdit;
    m_grade = new QComboBox;
    m_grade->addItems(Personnel::grades());
    m_poste = new QComboBox;
    m_poste->addItems(Personnel::postes());
    m_telephone = new QLineEdit;
    m_telephone->setPlaceholderText("8 chiffres, ex : 22 000 111");
    m_email = new QLineEdit;
    m_email->setPlaceholderText("prenom.nom@firestation.tn");
    m_motDePasse = new QLineEdit;
    m_motDePasse->setEchoMode(QLineEdit::Password);
    m_motDePasse->setPlaceholderText("4 caractères minimum");
    m_disponibilite = new QComboBox;
    m_disponibilite->addItems(Personnel::disponibilites());
    m_missions = new QSpinBox;
    m_missions->setMaximum(10);
    m_missions->setToolTip("Chaque mission en cours ajoute 20 % de charge opérationnelle");

    QLabel *labelTitre = new QLabel(titre);
    labelTitre->setProperty("class", "cardTitle");

    // Disposition : libellé + champ sur chaque ligne
    QFormLayout *formulaire = new QFormLayout;
    formulaire->addRow("ID :", new QLabel(m_id));
    formulaire->addRow("Nom :", m_nom);
    formulaire->addRow("Prénom :", m_prenom);
    formulaire->addRow("Grade :", m_grade);
    formulaire->addRow("Poste :", m_poste);
    formulaire->addRow("Téléphone :", m_telephone);
    formulaire->addRow("Email :", m_email);
    formulaire->addRow("Mot de passe :", m_motDePasse);
    formulaire->addRow("Disponibilité :", m_disponibilite);
    formulaire->addRow("Missions en cours :", m_missions);

    // Boutons
    QPushButton *boutonOk = new QPushButton("Enregistrer");
    QPushButton *boutonAnnuler = new QPushButton("Annuler");
    boutonOk->setProperty("class", "btnSuccess");
    boutonAnnuler->setProperty("class", "btnSecondary");
    connect(boutonOk, SIGNAL(clicked()), this, SLOT(valider()));
    connect(boutonAnnuler, SIGNAL(clicked()), this, SLOT(reject()));

    QHBoxLayout *ligneBoutons = new QHBoxLayout;
    ligneBoutons->addStretch();
    ligneBoutons->addWidget(boutonOk);
    ligneBoutons->addWidget(boutonAnnuler);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->setContentsMargins(24, 20, 24, 20);
    principal->setSpacing(12);
    principal->addWidget(labelTitre);
    principal->addLayout(formulaire);
    principal->addLayout(ligneBoutons);
    repolish(this);
}

void PersonnelDialog::setPersonnel(const Personnel &p)
{
    m_id = p.id();
    m_nom->setText(p.nom());
    m_prenom->setText(p.prenom());
    m_grade->setCurrentText(p.grade());
    m_poste->setCurrentText(p.poste());
    m_telephone->setText(p.telephone());
    m_email->setText(p.email());
    m_motDePasse->setText(p.motDePasse());
    m_disponibilite->setCurrentText(p.disponibilite());
    m_missions->setValue(p.missionsEnCours());
}

Personnel PersonnelDialog::personnel() const
{
    return Personnel(m_id, m_nom->text().trimmed(), m_prenom->text().trimmed(),
                   m_grade->currentText(), m_poste->currentText(),
                   m_telephone->text().trimmed(), m_email->text().trimmed(),
                   m_motDePasse->text(), m_disponibilite->currentText(),
                   m_missions->value());
}

void PersonnelDialog::valider()
{
    // Le téléphone doit contenir exactement 8 chiffres (les espaces sont ignorés)
    QString tel = m_telephone->text();
    tel.remove(" ");
    bool telEstUnNombre;
    tel.toLongLong(&telEstUnNombre);

    QString email = m_email->text().trimmed();

    QString erreur = "";
    if (m_nom->text().trimmed() == "")
        erreur = "Le nom est obligatoire.";
    else if (m_prenom->text().trimmed() == "")
        erreur = "Le prénom est obligatoire.";
    else if (tel.length() != 8 || !telEstUnNombre)
        erreur = "Le téléphone doit contenir exactement 8 chiffres.";
    else if (!email.contains("@") || !email.contains("."))
        erreur = "L'adresse email n'est pas valide.";
    else if (m_motDePasse->text().length() < 4)
        erreur = "Le mot de passe doit contenir au moins 4 caractères.";

    if (erreur != "") {
        QMessageBox::warning(this, "Saisie incorrecte", erreur);
        return; // on reste dans le formulaire
    }
    accept(); // tout est correct : on ferme le formulaire (résultat "Accepted")
}

// ===== vehiculedialog =====

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

VehiculeDialog::VehiculeDialog(const QString &titre, const QString &id,
                               const QStringList &immatriculationsExistantes, QWidget *parent)
    : QDialog(parent)
{
    m_id = id;
    m_existantes = immatriculationsExistantes;
    setWindowTitle(titre);
    setMinimumWidth(420);

    // Champs du formulaire
    m_immatriculation = new QLineEdit;
    m_immatriculation->setPlaceholderText("ex : 123 TU 456");
    m_type = new QComboBox;
    m_type->addItems(Vehicule::types());
    m_modele = new QLineEdit;
    m_etat = new QComboBox;
    m_etat->addItems(Vehicule::etats());
    m_disponibilite = new QComboBox;
    m_disponibilite->addItems(Vehicule::disponibilites());
    m_dateMaintenance = new QDateEdit(QDate::currentDate().addMonths(3));
    m_dateMaintenance->setCalendarPopup(true);
    m_dateMaintenance->setDisplayFormat("dd/MM/yyyy");

    QLabel *labelTitre = new QLabel(titre);
    labelTitre->setProperty("class", "cardTitle");

    // Disposition : libellé + champ sur chaque ligne
    QFormLayout *formulaire = new QFormLayout;
    formulaire->addRow("ID :", new QLabel(m_id));
    formulaire->addRow("Immatriculation :", m_immatriculation);
    formulaire->addRow("Type :", m_type);
    formulaire->addRow("Modèle :", m_modele);
    formulaire->addRow("État :", m_etat);
    formulaire->addRow("Disponibilité :", m_disponibilite);
    formulaire->addRow("Date de maintenance :", m_dateMaintenance);

    // Boutons
    QPushButton *boutonOk = new QPushButton("Enregistrer");
    QPushButton *boutonAnnuler = new QPushButton("Annuler");
    boutonOk->setProperty("class", "btnSuccess");
    boutonAnnuler->setProperty("class", "btnSecondary");
    connect(boutonOk, SIGNAL(clicked()), this, SLOT(valider()));
    connect(boutonAnnuler, SIGNAL(clicked()), this, SLOT(reject()));

    QHBoxLayout *ligneBoutons = new QHBoxLayout;
    ligneBoutons->addStretch();
    ligneBoutons->addWidget(boutonOk);
    ligneBoutons->addWidget(boutonAnnuler);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->setContentsMargins(24, 20, 24, 20);
    principal->setSpacing(12);
    principal->addWidget(labelTitre);
    principal->addLayout(formulaire);
    principal->addLayout(ligneBoutons);
    repolish(this);
}

void VehiculeDialog::setVehicule(const Vehicule &v)
{
    m_id = v.id();
    m_immatriculation->setText(v.immatriculation());
    m_type->setCurrentText(v.type());
    m_modele->setText(v.modele());
    m_etat->setCurrentText(v.etat());
    m_disponibilite->setCurrentText(v.disponibilite());
    m_dateMaintenance->setDate(v.dateMaintenance());
}

Vehicule VehiculeDialog::vehicule() const
{
    return Vehicule(m_id, m_immatriculation->text().trimmed().toUpper(), m_type->currentText(),
                    m_modele->text().trimmed(), m_etat->currentText(),
                    m_disponibilite->currentText(), m_dateMaintenance->date());
}

void VehiculeDialog::valider()
{
    QString immat = m_immatriculation->text().trimmed().toUpper();

    QString erreur = "";
    if (immat == "")
        erreur = "L'immatriculation est obligatoire.";
    else if (m_existantes.contains(immat))
        erreur = "Cette immatriculation existe déjà.";
    else if (m_modele->text().trimmed() == "")
        erreur = "Le modèle est obligatoire.";

    if (erreur != "") {
        QMessageBox::warning(this, "Saisie incorrecte", erreur);
        return; // on reste dans le formulaire
    }
    accept(); // tout est correct : on ferme le formulaire (résultat "Accepted")
}

// ===== gsmartfirestation =====


#include <QDesktopServices>
#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

GSmartFireStation::GSmartFireStation(QWidget *parent)
     : QMainWindow(parent), ui(new Ui::GSmartFireStation), m_personnelEditIndex(-1)
{
    ui->setupUi(this);
#ifdef STANDALONE_PERSONNEL
    // Projet individuel Personnel : garder uniquement son interface accessible.
    ui->btnNavDashboard->hide();
    ui->btnNavVehicules->hide();
    setWindowTitle("Gestion du Personnel - Smart Fire Station");
#elif defined(STANDALONE_VEHICULES)
    // Projet individuel Véhicules : garder uniquement son interface accessible.
    ui->btnNavDashboard->hide();
    ui->btnNavPersonnel->hide();
    setWindowTitle("Gestion des Véhicules - Smart Fire Station");
#endif
    repolish(this); // applique les class="..." du .ui au fichier style.qss

    setWindowIcon(QIcon(":/images/icon.png"));
    QPixmap logo(":/images/logo.png");
    ui->labelLogo->setPixmap(logo.scaledToWidth(192, Qt::SmoothTransformation));

    // Préparation de chaque module (le code vient des projets individuels)
    ui->frameSurcharge->hide();
    setupPersonnel();
    setupVehicules();

    // La connexion est facultative au démarrage : si Oracle n'est pas disponible,
    // l'application reste utilisable avec les données de démonstration.
    if (DatabaseManager::connectInteractive(this)) {
        chargerPersonnelExemple(); // cette fonction charge Oracle si connecté
        chargerVehiculesExemple();
    } else {
        chargerPersonnelExemple();
        chargerVehiculesExemple();
        statusBar()->showMessage("Mode démonstration : aucune connexion Oracle active.", 10000);
    }
    refreshPersonnelTable();
    refreshVehiculesTable();

#ifdef STANDALONE_PERSONNEL
    goToPage(1); // application individuelle : Personnel uniquement
#elif defined(STANDALONE_VEHICULES)
    goToPage(2); // application individuelle : Véhicules uniquement
#else
    goToPage(0); // application intégrée : Dashboard
#endif
}

GSmartFireStation::~GSmartFireStation()
{
    DatabaseManager::close();
    delete ui;
}

// ===================== NAVIGATION =====================
// Affiche la page d'index donné du QStackedWidget : 0 = Dashboard, 1 = Personnel, 2 = Véhicules
void GSmartFireStation::goToPage(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
    statusBar()->clearMessage();

    // Le menu latéral montre la page affichée
    ui->btnNavDashboard->setChecked(index == 0);
    ui->btnNavPersonnel->setChecked(index == 1);
    ui->btnNavVehicules->setChecked(index == 2);

    if (index == 0)
        refreshDashboard(); // le Dashboard est recalculé à chaque affichage
}

void GSmartFireStation::on_btnNavDashboard_clicked()
{
    goToPage(0);
}

void GSmartFireStation::on_btnNavPersonnel_clicked()
{
    goToPage(1);
}

void GSmartFireStation::on_btnNavVehicules_clicked()
{
    goToPage(2);
}

void GSmartFireStation::on_btnGererPersonnel_clicked()
{
    goToPage(1);
}

void GSmartFireStation::on_btnGererVehicules_clicked()
{
    goToPage(2);
}

void GSmartFireStation::on_btnRetourPersonnel_clicked()
{
#ifdef STANDALONE_PERSONNEL
    goToPage(1);
#else
    goToPage(0);
#endif
}

void GSmartFireStation::on_btnRetourVehicule_clicked()
{
#ifdef STANDALONE_VEHICULES
    goToPage(2);
#else
    goToPage(0);
#endif
}

void GSmartFireStation::on_btnQuitter_clicked()
{
    int reponse = QMessageBox::question(this, "Quitter", "Voulez-vous vraiment quitter l'application ?");
    if (reponse == QMessageBox::Yes)
        close();
}

// ===================== DASHBOARD =====================
// Met à jour une barre de progression et son texte "valeur/total"
void GSmartFireStation::majBarre(QProgressBar *barre, QLabel *label, int valeur, int total)
{
    label->setText(QString("%1/%2").arg(valeur).arg(total));
    if (total < 1)
        total = 1;
    barre->setMaximum(total);
    barre->setValue(valeur);
}

// Ajoute une ligne dans la liste des alertes (texte coloré)
void GSmartFireStation::ajouterAlerte(const QString &texte, const QString &couleur)
{
    QListWidgetItem *ligne = new QListWidgetItem("●  " + texte);
    ligne->setForeground(QColor(couleur));
    ui->listAlertes->addItem(ligne);
}

void GSmartFireStation::refreshDashboard()
{
    // --- Comptage du personnel ---
    int personnelDispo = 0;
    int personnelMission = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        if (m_personnel[i].disponibilite() == "Disponible")
            personnelDispo++;
        else if (m_personnel[i].disponibilite() == "En mission")
            personnelMission++;
    }

    // --- Comptage des véhicules ---
    int vehiculesDispo = 0;
    int vehiculesMaintenance = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() == "Disponible")
            vehiculesDispo++;
        else if (m_vehicules[i].disponibilite() == "En maintenance")
            vehiculesMaintenance++;
    }

    int nbPersonnel = m_personnel.size();
    int nbVehicules = m_vehicules.size();

    // --- Cartes du haut ---
    ui->labelNbPersonnel->setText(QString::number(nbPersonnel));
    ui->labelPersonnelDispoDetail->setText(QString("Disponibles : %1").arg(personnelDispo));
    ui->labelNbVehicules->setText(QString::number(nbVehicules));
    ui->labelVehiculesDispoDetail->setText(QString("Disponibles : %1").arg(vehiculesDispo));

    // --- Barres de disponibilité ---
    majBarre(ui->progressPersonnelDispo, ui->labelPersonnelDispoCount, personnelDispo, nbPersonnel);
    majBarre(ui->progressPersonnelMission, ui->labelPersonnelMissionCount, personnelMission, nbPersonnel);
    majBarre(ui->progressVehiculesDispo, ui->labelVehiculesDispoCount, vehiculesDispo, nbVehicules);
    majBarre(ui->progressVehiculesMaint, ui->labelVehiculesMaintCount, vehiculesMaintenance, nbVehicules);

    // --- Alertes ---
    ui->listAlertes->clear();
    int nbAlertes = 0;
    int seuil = ui->spinSeuil->value();
    for (int i = 0; i < m_personnel.size(); i++) {
        if (m_personnel[i].charge() >= seuil) {
            ajouterAlerte(QString("Personnel %1 : charge opérationnelle élevée (%2 %)")
                              .arg(m_personnel[i].id()).arg(m_personnel[i].charge()), "#C00000");
            nbAlertes++;
        }
    }
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() != "En maintenance" && m_vehicules[i].maintenanceProche()) {
            ajouterAlerte(QString("Véhicule %1 : maintenance à prévoir (%2)")
                              .arg(m_vehicules[i].id())
                              .arg(m_vehicules[i].dateMaintenance().toString("dd/MM/yyyy")), "#C00000");
            nbAlertes++;
        }
    }
    ajouterAlerte("Préparer un véhicule pour une intervention", "#B45309");
    ui->labelNbAlertes->setText(QString::number(nbAlertes));
}

// Bouton "Voir les statistiques" : statistiques des deux modules dans une seule fenêtre
void GSmartFireStation::on_btnVoirStats_clicked()
{
    StatCritere c1 = statPersonnelParDispo();
    c1.nom = "Personnel — Disponibilité";
    StatCritere c2 = statPersonnelParGrade();
    c2.nom = "Personnel — Grade";
    StatCritere c3 = statVehiculesParDispo();
    c3.nom = "Véhicules — Disponibilité";
    StatCritere c4 = statVehiculesParEtat();
    c4.nom = "Véhicules — État";

    QList<StatCritere> criteres;
    criteres << c1 << c2 << c3 << c4;
    StatsDialog fenetre("Statistiques de la station", criteres, this);
    fenetre.exec();
}

// ===================== OUTILS COMMUNS =====================

// Crée une case de tableau avec une couleur de texte (et éventuellement en gras)
QTableWidgetItem *GSmartFireStation::creerItem(const QString &texte, const QColor &couleur, bool gras)
{
    QTableWidgetItem *item = new QTableWidgetItem(texte);
    item->setForeground(couleur);
    if (gras) {
        QFont police = item->font();
        police.setBold(true);
        item->setFont(police);
    }
    return item;
}

// Change la "class" d'un label (pour qu'il prenne un autre style du fichier style.qss)
void GSmartFireStation::setLabelClass(QLabel *label, const QString &classe)
{
    label->setProperty("class", classe);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Exporte en PDF les lignes visibles d'un tableau (donc le résultat d'une recherche / d'un tri)
void GSmartFireStation::exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier)
{
    QStringList entetes;
    for (int c = 0; c < table->columnCount(); c++)
        entetes << table->horizontalHeaderItem(c)->text();

    QList<QStringList> lignes;
    for (int r = 0; r < table->rowCount(); r++) {
        if (table->isRowHidden(r))
            continue;
        QStringList ligne;
        for (int c = 0; c < table->columnCount(); c++)
            ligne << table->item(r, c)->text();
        lignes << ligne;
    }
    if (lignes.size() == 0) {
        QMessageBox::information(this, "Export PDF", "Aucune donnée à exporter.");
        return;
    }

    QString dossier = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString chemin = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                                  dossier + "/" + nomFichier, "Fichiers PDF (*.pdf)");
    if (chemin == "")
        return; // l'utilisateur a annulé
    if (!chemin.endsWith(".pdf"))
        chemin += ".pdf";

    if (!creerPdfTableau(chemin, titre, entetes, lignes)) {
        QMessageBox::warning(this, "Export PDF", "Impossible de créer le fichier PDF.");
        return;
    }
    statusBar()->showMessage("PDF exporté : " + chemin, 6000);
    int reponse = QMessageBox::question(this, "Export PDF",
                                        "Le PDF a été créé.\nVoulez-vous l'ouvrir maintenant ?");
    if (reponse == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(chemin));
}

// ===== gsmartfirestation_personnel =====


#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QScrollArea>

// =====================================================================
//                        MODULE PERSONNEL
// =====================================================================

void GSmartFireStation::setupPersonnel()
{
    // Colonnes du tableau : ID étroite, les autres se partagent la place
    QHeaderView *entete = ui->tableWidgetPersonnel->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);
    entete->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Formulaire permanent : l'ajout et la modification se font directement dans la page.
    m_formPersonnel = new QGroupBox("Fiche du personnel — remplissez les champs puis validez");
    m_formPersonnel->setObjectName("formPersonnelInline");
    m_formPersonnel->setStyleSheet("QGroupBox { border: 1px solid #D7DEE8; border-radius: 8px; margin-top: 10px; padding: 10px; font-weight: 600; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #1F4E79; }");
    m_formId = new QLabel(prochainIdPersonnel());
    m_formNom = new QLineEdit; m_formNom->setPlaceholderText("Nom");
    m_formPrenom = new QLineEdit; m_formPrenom->setPlaceholderText("Prénom");
    m_formGrade = new QComboBox; m_formGrade->addItems(Personnel::grades());
    m_formPoste = new QComboBox; m_formPoste->addItems(Personnel::postes());
    m_formTelephone = new QLineEdit; m_formTelephone->setPlaceholderText("8 chiffres");
    m_formEmail = new QLineEdit; m_formEmail->setPlaceholderText("prenom.nom@firestation.tn");
    m_formMotDePasse = new QLineEdit; m_formMotDePasse->setPlaceholderText("4 caractères minimum"); m_formMotDePasse->setEchoMode(QLineEdit::Password);
    m_formDisponibilite = new QComboBox; m_formDisponibilite->addItems(Personnel::disponibilites());
    m_formMissions = new QSpinBox; m_formMissions->setRange(0, 10); m_formMissions->setToolTip("Chaque mission ajoute 20 % de charge opérationnelle");
    // Compact, permanent editor: fields are arranged horizontally in two rows.
    QGridLayout *formGrid = new QGridLayout(m_formPersonnel);
    formGrid->setContentsMargins(8, 12, 8, 8); formGrid->setHorizontalSpacing(8); formGrid->setVerticalSpacing(5);
    auto addField = [&](int row, int col, const QString &name, QWidget *field) {
        QLabel *label = new QLabel(name); label->setStyleSheet("font-weight: normal;");
        QVBoxLayout *cell = new QVBoxLayout; cell->setContentsMargins(0,0,0,0); cell->setSpacing(2); cell->addWidget(label); cell->addWidget(field);
        QWidget *wrap = new QWidget; wrap->setLayout(cell); formGrid->addWidget(wrap, row, col);
    };
    addField(0,0,"ID",m_formId); addField(0,1,"Nom",m_formNom); addField(0,2,"Prénom",m_formPrenom);
    addField(0,3,"Grade",m_formGrade); addField(0,4,"Poste",m_formPoste);
    addField(1,0,"Téléphone",m_formTelephone); addField(1,1,"Email",m_formEmail); addField(1,2,"Mot de passe",m_formMotDePasse);
    addField(1,3,"Disponibilité",m_formDisponibilite); addField(1,4,"Missions",m_formMissions);
    for (int c=0;c<5;++c) formGrid->setColumnStretch(c,1);
    QPushButton *valider = new QPushButton("Enregistrer"); valider->setObjectName("btnValiderPersonnelInline"); valider->setStyleSheet("QPushButton { background:#1F4E79; color:white; border-radius:5px; padding:6px 10px; font-weight:600; }");
    QPushButton *annuler = new QPushButton("Effacer"); annuler->setObjectName("btnAnnulerPersonnelInline");
    QHBoxLayout *actions = new QHBoxLayout; actions->addWidget(valider); actions->addWidget(annuler); formGrid->addLayout(actions, 2, 0, 1, 5);
    ui->verticalLayoutPersonnelLeft->removeWidget(m_formPersonnel);
    ui->verticalLayoutPersonnelLeft->addWidget(m_formPersonnel);
    // Keep the compact editor permanently visible below the table/actions.
    m_formPersonnel->show();
    // Déplacer les actions CRUD dans la colonne de droite, sous le formulaire.
    QHBoxLayout *crud = ui->horizontalLayoutCrudPersonnel;
    crud->removeWidget(ui->btnAjouterPersonnel); crud->removeWidget(ui->btnModifierPersonnel); crud->removeWidget(ui->btnSupprimerPersonnel);
    ui->verticalLayoutPersonnelLeft->removeItem(crud);
    delete crud;
    QWidget *actionsCrud = new QWidget;
    QHBoxLayout *crudLayout = new QHBoxLayout(actionsCrud);
    crudLayout->setContentsMargins(0, 8, 0, 0);
    crudLayout->setSpacing(10);
    crudLayout->addWidget(ui->btnAjouterPersonnel);
    crudLayout->addWidget(ui->btnModifierPersonnel);
    crudLayout->addWidget(ui->btnSupprimerPersonnel);
    crudLayout->addStretch(1);
    // Keep the table in the main area and put its actions directly underneath.
    ui->verticalLayoutPersonnelLeft->insertWidget(ui->verticalLayoutPersonnelLeft->indexOf(m_formPersonnel), actionsCrud);
    connect(valider, SIGNAL(clicked()), this, SLOT(enregistrerPersonnelInline()));
    connect(annuler, SIGNAL(clicked()), this, SLOT(annulerPersonnelInline()));

    // Statistiques circulaires toujours visibles, avec une légende par grade.
    ui->horizontalLayoutStatDispo->setEnabled(false); ui->labelStatDispo->hide(); ui->progressStatDispo->hide();
    ui->horizontalLayoutStatMission->setEnabled(false); ui->labelStatMission->hide(); ui->progressStatMission->hide();
    m_personnelStatsChart = new PieChartWidget("PERSONNEL", ui->frameStatsPersonnel);
    m_personnelStatsLegend = new QLabel(ui->frameStatsPersonnel);
    m_personnelStatsLegend->setWordWrap(true);
    m_personnelStatsLegend->hide();
    m_personnelStatsChart->setMinimumHeight(390);
    QScrollArea *statsScroll = new QScrollArea(ui->frameStatsPersonnel);
    statsScroll->setWidgetResizable(true);
    statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    statsScroll->setFrameShape(QFrame::NoFrame);
    QWidget *statsContent = new QWidget;
    QVBoxLayout *statsContentLayout = new QVBoxLayout(statsContent);
    statsContentLayout->setContentsMargins(4, 4, 4, 4);
    statsContentLayout->addWidget(m_personnelStatsChart);
    statsScroll->setWidget(statsContent);
    ui->verticalLayoutStatsPersonnel->insertWidget(1, statsScroll, 1);
    ui->frameStatsPersonnel->setMinimumWidth(285);
    ui->btnStatsPersonnel->hide();
    ui->frameSurcharge->hide();
    ui->frameStatsPersonnel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

}

// Données de démonstration (à remplacer plus tard par la base de données)
void GSmartFireStation::chargerPersonnelExemple()
{
    m_personnel.clear();
    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("SELECT p.id_personnel, p.nom, p.prenom, p.grade, p.poste, "
                  "p.telephone, p.email, p.disponibilite, "
                  "(SELECT COUNT(*) FROM mission_personnel mp JOIN mission m "
                  "ON m.id_mission=mp.id_mission WHERE mp.id_personnel=p.id_personnel "
                  "AND UPPER(m.etat) IN ('EN COURS','EN_COURS','EN COURS')) "
                  "FROM personnel p ORDER BY p.id_personnel");
        if (q.exec()) {
            while (q.next()) {
                QString id = QString("P%1").arg(q.value(0).toInt(), 3, 10, QChar('0'));
                m_personnel.append(Personnel(id, q.value(1).toString(), q.value(2).toString(),
                    q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                    q.value(6).toString(), QString(), q.value(7).toString(), q.value(8).toInt()));
            }
            return;
        }
        QMessageBox::warning(this, "Erreur Oracle",
            "Impossible de charger PERSONNEL : " + q.lastError().text());
    }

    // Mode démonstration si Oracle n'est pas connecté.
    m_personnel.append(Personnel("P001", "Ben Ali", "Ahmed", "Capitaine", "Chef de garde", "22 000 111",
                              "ahmed.benali@firestation.tn", "", "Disponible", 1));
    m_personnel.append(Personnel("P002", "Trabelsi", "Yassine", "Sergent", "Intervention", "22 000 222",
                              "yassine.trabelsi@firestation.tn", "", "En mission", 3));
    m_personnel.append(Personnel("P003", "Jlassi", "Sami", "Caporal", "Secours", "22 000 333",
                              "sami.jlassi@firestation.tn", "", "Disponible", 0));
    m_personnel.append(Personnel("P004", "Gharbi", "Mehdi", "Lieutenant", "Intervention", "22 000 555",
                              "mehdi.gharbi@firestation.tn", "", "Disponible", 1));
    m_personnel.append(Personnel("P005", "Khelifi", "Walid", "Sapeur", "Secours", "22 000 666",
                              "walid.khelifi@firestation.tn", "", "Disponible", 2));
    m_personnel.append(Personnel("P006", "Ben Salah", "Karim", "Adjudant", "Logistique", "22 000 777",
                              "karim.bensalah@firestation.tn", "", "En congé", 0));
    m_personnel.append(Personnel("P007", "Dridi", "Nour", "Sapeur", "Intervention", "22 000 888",
                              "nour.dridi@firestation.tn", "", "En mission", 4));
    m_personnel.append(Personnel("P014", "Mansour", "Omar", "Sergent", "Intervention", "22 000 444",
                              "omar.mansour@firestation.tn", "", "En mission", 5));
}

// Numéro de la ligne sélectionnée dans le tableau (-1 si aucune).
// La ligne n du tableau correspond toujours au personnel n de la liste m_personnel.
int GSmartFireStation::personnelSelectionne()
{
    int ligne = ui->tableWidgetPersonnel->currentRow();
    if (ligne < 0 || ligne >= m_personnel.size())
        return -1;
    return ligne;
}

// Prochain ID libre : P001, P002, ...
QString GSmartFireStation::prochainIdPersonnel()
{
    int max = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        int numero = m_personnel[i].id().mid(1).toInt();
        if (numero > max)
            max = numero;
    }
    return QString("P%1").arg(max + 1, 3, 10, QChar('0'));
}

// ------------------------------------------------------------------ affichage
void GSmartFireStation::refreshPersonnelTable()
{
    int seuil = ui->spinSeuil->value();
    int ligneCourante = ui->tableWidgetPersonnel->currentRow();
    QColor noir("#1F1F1F");

    ui->tableWidgetPersonnel->setRowCount(m_personnel.size());
    for (int r = 0; r < m_personnel.size(); r++) {
        Personnel p = m_personnel[r];

        // Nom et prénom en rouge gras si le personnel est en surcharge
        QColor couleurNom = noir;
        bool gras = false;
        if (p.charge() >= seuil) {
            couleurNom = QColor("#C00000");
            gras = true;
        }

        // Couleur de la disponibilité
        QColor couleurDispo("#6B7280");
        if (p.disponibilite() == "Disponible")
            couleurDispo = QColor("#2E7D32");
        else if (p.disponibilite() == "En mission")
            couleurDispo = QColor("#B45309");

        ui->tableWidgetPersonnel->setItem(r, 0, creerItem(p.id(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 1, creerItem(p.nom(), couleurNom, gras));
        ui->tableWidgetPersonnel->setItem(r, 2, creerItem(p.prenom(), couleurNom, gras));
        ui->tableWidgetPersonnel->setItem(r, 3, creerItem(p.grade(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 4, creerItem(p.poste(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 5, creerItem(p.telephone(), noir, false));
        ui->tableWidgetPersonnel->setItem(r, 6, creerItem(p.disponibilite(), couleurDispo, true));
    }
    if (ligneCourante >= 0 && ligneCourante < m_personnel.size())
        ui->tableWidgetPersonnel->selectRow(ligneCourante);

    filtrerPersonnel();

    // Panneau "Statistiques"
    int dispo = 0;
    int mission = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        if (m_personnel[i].disponibilite() == "Disponible")
            dispo++;
        else if (m_personnel[i].disponibilite() == "En mission")
            mission++;
    }
    int total = m_personnel.size();
    int maximum = total;
    if (maximum < 1)
        maximum = 1;
    ui->progressStatDispo->setMaximum(maximum);
    ui->progressStatDispo->setValue(dispo);
    ui->progressStatMission->setMaximum(maximum);
    ui->progressStatMission->setValue(mission);
    ui->labelTotalPersonnel->setText(QString("Total : %1 personnel(s)").arg(total));
    QStringList grades = Personnel::grades();
    QStringList gradeLabels; QList<int> gradeValues;
    for (int g=0; g<grades.size(); ++g) {
        int count=0;
        for (int i=0; i<m_personnel.size(); ++i) if (m_personnel[i].grade()==grades[g]) ++count;
        gradeLabels << grades[g]; gradeValues << count;
    }
    static_cast<PieChartWidget *>(m_personnelStatsChart)->setData(gradeLabels, gradeValues);
    int pctDispo = total ? qRound(100.0 * dispo / total) : 0;
    int pctMission = total ? qRound(100.0 * mission / total) : 0;
    int pctConge = total ? qRound(100.0 * (total-dispo-mission) / total) : 0;
    m_personnelStatsLegend->setText(QString("Disponibles : %1 (%2 %)\nEn mission : %3 (%4 %)\nEn congé / autres : %5 (%6 %)")
        .arg(dispo).arg(pctDispo).arg(mission).arg(pctMission).arg(total-dispo-mission).arg(pctConge));

    refreshSurcharge();
}

// Métier innovant : détection de surcharge
void GSmartFireStation::refreshSurcharge()
{
    int seuil = ui->spinSeuil->value();

    int plusCharge = -1;  // numéro du personnel le plus chargé (parmi ceux en surcharge)
    int nbSurcharge = 0;
    int chargeMax = 0;
    for (int i = 0; i < m_personnel.size(); i++) {
        int c = m_personnel[i].charge();
        if (c > chargeMax)
            chargeMax = c;
        if (c >= seuil) {
            nbSurcharge++;
            if (plusCharge < 0 || c > m_personnel[plusCharge].charge())
                plusCharge = i;
        }
    }

    if (plusCharge < 0) { // personne en surcharge
        ui->labelSurchargeNom->setText("Aucun personnel en surcharge");
        ui->labelSurchargeCharge->setText(QString("Charge maximale : %1 %").arg(chargeMax));
        setLabelClass(ui->labelSurchargeCharge, "cardDetail");
        ui->labelSurchargeEtat->setText("✔ Charge opérationnelle normale");
        setLabelClass(ui->labelSurchargeEtat, "textSuccessBold");
        ui->btnDetailsSurcharge->setEnabled(false);
        return;
    }

    Personnel p = m_personnel[plusCharge];
    ui->labelSurchargeNom->setText(p.id() + " — " + p.nomComplet());
    ui->labelSurchargeCharge->setText(QString("Charge actuelle : %1 %").arg(p.charge()));
    setLabelClass(ui->labelSurchargeCharge, "textDanger");
    QString etat = "⚠ Surcharge détectée";
    if (nbSurcharge > 1)
        etat += QString(" (+%1 autre(s))").arg(nbSurcharge - 1);
    ui->labelSurchargeEtat->setText(etat);
    setLabelClass(ui->labelSurchargeEtat, "textDangerBold");
    ui->btnDetailsSurcharge->setEnabled(true);
}

// ------------------------------------------------------------------ CRUD
void GSmartFireStation::on_btnAjouterPersonnel_clicked()
{
    annulerPersonnelInline();
    m_personnelEditIndex = -1;
    m_formId->setText(prochainIdPersonnel());
    m_formPersonnel->setTitle("Ajouter un personnel — remplissez la fiche ci-dessous");
    // Editor remains visible; only the field contents change.
    m_formPersonnel->show();
    m_formNom->setFocus();
    statusBar()->showMessage("Remplissez le formulaire visible puis cliquez sur Enregistrer.", 5000);
}

void GSmartFireStation::on_btnModifierPersonnel_clicked()
{
    int i = personnelSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Modifier un personnel", "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    const Personnel p = m_personnel[i];
    m_personnelEditIndex = i;
    m_formId->setText(p.id());
    m_formNom->setText(p.nom()); m_formPrenom->setText(p.prenom());
    m_formGrade->setCurrentText(p.grade()); m_formPoste->setCurrentText(p.poste());
    m_formTelephone->setText(p.telephone()); m_formEmail->setText(p.email());
    m_formMotDePasse->setText(p.motDePasse()); m_formDisponibilite->setCurrentText(p.disponibilite());
    m_formMissions->setValue(p.missionsEnCours());
    m_formPersonnel->setTitle("Modifier " + p.id() + " — modifiez les champs puis enregistrez");
    m_formPersonnel->show();
    m_formNom->setFocus();
}

void GSmartFireStation::enregistrerPersonnelInline()
{
    QString tel = m_formTelephone->text(); tel.remove(" "); tel.remove("-");
    bool telOk = false; tel.toLongLong(&telOk);
    QString email = m_formEmail->text().trimmed();
    QString erreur;
    if (m_formNom->text().trimmed().isEmpty()) erreur = "Le nom est obligatoire.";
    else if (m_formPrenom->text().trimmed().isEmpty()) erreur = "Le prénom est obligatoire.";
    else if (tel.length() != 8 || !telOk) erreur = "Le téléphone doit contenir exactement 8 chiffres.";
    else if (!email.contains("@") || !email.contains(".")) erreur = "L'adresse email n'est pas valide.";
    if (!erreur.isEmpty()) { QMessageBox::warning(this, "Saisie incorrecte", erreur); return; }

    const bool editing = m_personnelEditIndex >= 0 && m_personnelEditIndex < m_personnel.size();
    QString id = editing ? m_personnel[m_personnelEditIndex].id() : QString();
    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        if (editing) {
            q.prepare("UPDATE personnel SET nom=:nom, prenom=:prenom, grade=:grade, poste=:poste, "
                      "telephone=:tel, email=:email, disponibilite=:dispo WHERE id_personnel=:id");
            q.bindValue(":id", id.mid(1).toInt());
        } else {
            q.prepare("INSERT INTO personnel (nom, prenom, grade, poste, telephone, email, disponibilite) "
                      "VALUES (:nom, :prenom, :grade, :poste, :tel, :email, :dispo)");
        }
        q.bindValue(":nom", m_formNom->text().trimmed());
        q.bindValue(":prenom", m_formPrenom->text().trimmed());
        q.bindValue(":grade", m_formGrade->currentText());
        q.bindValue(":poste", m_formPoste->currentText());
        q.bindValue(":tel", tel);
        q.bindValue(":email", email);
        q.bindValue(":dispo", m_formDisponibilite->currentText());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Enregistrement du personnel impossible : " + q.lastError().text());
            return;
        }
        if (!editing) {
            QSqlQuery idQuery(DatabaseManager::database());
            if (!idQuery.exec("SELECT seq_personnel.CURRVAL FROM dual") || !idQuery.next()) {
                QMessageBox::warning(this, "Oracle", "Personnel enregistré, mais l'identifiant généré n'a pas pu être récupéré.");
                chargerPersonnelExemple();
            } else {
                id = QString("P%1").arg(idQuery.value(0).toInt(), 3, 10, QChar('0'));
            }
        }
        chargerPersonnelExemple();
        m_personnelEditIndex = -1;
        refreshPersonnelTable();
        m_formPersonnel->setTitle("Fiche du personnel — remplissez les champs puis validez");
        m_formPersonnel->show();
        statusBar()->showMessage(editing ? "Personnel modifié dans Oracle." : "Personnel ajouté dans Oracle.", 5000);
        return;
    }

    Personnel p(id.isEmpty() ? prochainIdPersonnel() : id, m_formNom->text().trimmed(), m_formPrenom->text().trimmed(),
                m_formGrade->currentText(), m_formPoste->currentText(), m_formTelephone->text().trimmed(),
                email, m_formMotDePasse->text(), m_formDisponibilite->currentText(), m_formMissions->value());
    if (editing) m_personnel[m_personnelEditIndex] = p;
    else { m_personnel.append(p); m_personnelEditIndex = m_personnel.size() - 1; }
    refreshPersonnelTable();
    ui->tableWidgetPersonnel->selectRow(m_personnelEditIndex);
    statusBar()->showMessage(editing ? "Personnel modifié (mode démonstration)." : "Personnel ajouté (mode démonstration).", 4000);
    m_formPersonnel->setTitle("Fiche du personnel — remplissez les champs puis validez");
}

void GSmartFireStation::annulerPersonnelInline()
{
    m_personnelEditIndex = -1;
    m_formId->setText(prochainIdPersonnel());
    m_formNom->clear(); m_formPrenom->clear(); m_formTelephone->clear();
    m_formEmail->clear(); m_formMotDePasse->clear(); m_formMissions->setValue(0);
    m_formGrade->setCurrentIndex(0); m_formPoste->setCurrentIndex(0); m_formDisponibilite->setCurrentText("Disponible");
    m_formPersonnel->setTitle("Fiche du personnel — remplissez les champs puis validez");
    m_formPersonnel->show();
}

void GSmartFireStation::on_btnSupprimerPersonnel_clicked()
{
    int i = personnelSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Supprimer un personnel",
                                 "Sélectionnez d'abord un personnel dans la liste.");
        return;
    }
    int reponse = QMessageBox::question(this, "Supprimer un personnel",
                                        "Voulez-vous vraiment supprimer " + m_personnel[i].nomComplet() + " ?");
    if (reponse != QMessageBox::Yes)
        return;

    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("DELETE FROM personnel WHERE id_personnel=:id");
        q.bindValue(":id", m_personnel[i].id().mid(1).toInt());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Suppression impossible : " + q.lastError().text());
            return;
        }
        chargerPersonnelExemple();
    } else {
        m_personnel.removeAt(i);
    }
    ui->tableWidgetPersonnel->setCurrentCell(-1, -1);
    refreshPersonnelTable();
    statusBar()->showMessage("Personnel supprimé.", 5000);
}

void GSmartFireStation::on_tableWidgetPersonnel_cellDoubleClicked(int, int)
{
    on_btnModifierPersonnel_clicked(); // double-clic = modifier
}

// ------------------------------------------------------------------ recherche
// Cache les lignes qui ne contiennent pas le texte cherché (ID, nom, prénom ou grade)
void GSmartFireStation::filtrerPersonnel()
{
    QString texte = ui->lineEditRecherchePersonnel->text().trimmed();
    int visibles = 0;
    for (int r = 0; r < m_personnel.size(); r++) {
        Personnel p = m_personnel[r];
        bool trouve = texte == ""
                      || p.id().contains(texte, Qt::CaseInsensitive)
                      || p.nom().contains(texte, Qt::CaseInsensitive)
                      || p.prenom().contains(texte, Qt::CaseInsensitive)
                      || p.grade().contains(texte, Qt::CaseInsensitive);
        ui->tableWidgetPersonnel->setRowHidden(r, !trouve);
        if (trouve)
            visibles++;
    }
    if (texte != "")
        statusBar()->showMessage(QString("%1 résultat(s)").arg(visibles), 4000);
    else
        statusBar()->clearMessage();
}

void GSmartFireStation::on_btnRechercherPersonnel_clicked()
{
    filtrerPersonnel();
}

void GSmartFireStation::on_lineEditRecherchePersonnel_textChanged(const QString &)
{
    filtrerPersonnel(); // la recherche se met à jour pendant la frappe
}

// ------------------------------------------------------------------ tri (tri à bulles)
// Retourne vrai si 'a' doit passer après 'b' selon le critère choisi
bool GSmartFireStation::doitEchangerPersonnel(Personnel a, Personnel b, int critere)
{
    if (critere == 0) // Grade : du plus élevé au moins élevé
        return Personnel::rangGrade(a.grade()) > Personnel::rangGrade(b.grade());
    if (critere == 1) // Disponibilité : Disponible, En mission, En congé
        return Personnel::disponibilites().indexOf(a.disponibilite())
               > Personnel::disponibilites().indexOf(b.disponibilite());
    return a.nom().toLower() > b.nom().toLower(); // Nom : ordre alphabétique
}

void GSmartFireStation::on_btnTrierPersonnel_clicked()
{
    int critere = ui->comboTriPersonnel->currentIndex();
    int n = m_personnel.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (doitEchangerPersonnel(m_personnel[j], m_personnel[j + 1], critere)) {
                Personnel temp = m_personnel[j];
                m_personnel[j] = m_personnel[j + 1];
                m_personnel[j + 1] = temp;
            }
        }
    }
    ui->tableWidgetPersonnel->setCurrentCell(-1, -1);
    refreshPersonnelTable();
    statusBar()->showMessage("Liste triée.", 4000);
}

// ------------------------------------------------------------------ export PDF
void GSmartFireStation::on_btnExportPersonnel_clicked()
{
    exporterTableauPdf(ui->tableWidgetPersonnel, "Liste du personnel", "personnel.pdf");
}

// ------------------------------------------------------------------ statistiques
StatCritere GSmartFireStation::statPersonnelParGrade()
{
    QStringList reelles;
    for (int i = 0; i < m_personnel.size(); i++)
        reelles << m_personnel[i].grade();
    return compterParValeur("Grade", Personnel::grades(), reelles);
}

StatCritere GSmartFireStation::statPersonnelParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_personnel.size(); i++)
        reelles << m_personnel[i].disponibilite();
    return compterParValeur("Disponibilité", Personnel::disponibilites(), reelles);
}

void GSmartFireStation::on_btnStatsPersonnel_clicked()
{
    return; // les statistiques sont toujours visibles dans le panneau latéral
    QList<StatCritere> criteres;
    criteres << statPersonnelParGrade() << statPersonnelParDispo();
    StatsDialog fenetre("Statistiques du personnel", criteres, this);
    fenetre.exec();
}

// ------------------------------------------------------------------ surcharge
void GSmartFireStation::on_spinSeuil_valueChanged(int)
{
    refreshPersonnelTable(); // le seuil a changé : on recalcule les surcharges
}

void GSmartFireStation::on_btnDetailsSurcharge_clicked()
{
    int seuil = ui->spinSeuil->value();
    QString texte = "";
    int nb = 0;
    int plusCharge = -1;
    for (int i = 0; i < m_personnel.size(); i++) {
        Personnel p = m_personnel[i];
        if (p.charge() >= seuil) {
            nb++;
            texte += QString("• %1 — %2 (%3) : %4 mission(s), charge %5 %\n")
                         .arg(p.id()).arg(p.nomComplet()).arg(p.grade())
                         .arg(p.missionsEnCours()).arg(p.charge());
            if (plusCharge < 0 || p.charge() > m_personnel[plusCharge].charge())
                plusCharge = i;
        }
    }
    if (nb == 0)
        return;

    QMessageBox boite(this);
    boite.setIcon(QMessageBox::Warning);
    boite.setWindowTitle("Personnel en surcharge");
    boite.setText(QString("%1 personnel(s) dépassent le seuil de %2 %.").arg(nb).arg(seuil));
    boite.setInformativeText(texte + "\nPensez à réaffecter une partie de leurs missions.");
    boite.exec();

    ui->tableWidgetPersonnel->selectRow(plusCharge); // montre le plus chargé dans le tableau
}

// ===== gsmartfirestation_vehicules =====

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QScrollArea>
#include <QPushButton>

#include <QHeaderView>
#include <QMessageBox>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>

// =====================================================================
//                        MODULE VÉHICULES
// =====================================================================

void GSmartFireStation::setupVehicules()
{
    m_vehiculePropose = -1;
    ui->frameMission->hide();

    // Colonnes du tableau : ID étroite, les autres se partagent la place
    QHeaderView *entete = ui->tableWidgetVehicules->horizontalHeader();
    entete->setSectionResizeMode(QHeaderView::Stretch);
    entete->setSectionResizeMode(0, QHeaderView::Fixed);
    entete->resizeSection(0, 70);
    entete->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_vehiculeEditIndex = -1;
    m_formVehicule = new QGroupBox("Fiche du véhicule", this);
    m_formVehicule->setStyleSheet("QGroupBox { border: 1px solid #D7DEE8; border-radius: 8px; margin-top: 10px; padding: 10px; font-weight: 600; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #1F4E79; }");
    m_vehicleId = new QLabel(prochainIdVehicule());
    m_vehicleImmat = new QLineEdit; m_vehicleImmat->setPlaceholderText("ex. 123 TU 456");
    m_vehicleType = new QComboBox; m_vehicleType->addItems(Vehicule::types());
    m_vehicleModele = new QLineEdit; m_vehicleModele->setPlaceholderText("Modèle / marque");
    m_vehicleEtat = new QComboBox; m_vehicleEtat->addItems(Vehicule::etats());
    m_vehicleDispo = new QComboBox; m_vehicleDispo->addItems(Vehicule::disponibilites());
    m_vehicleDate = new QDateEdit(QDate::currentDate().addMonths(3)); m_vehicleDate->setCalendarPopup(true); m_vehicleDate->setDisplayFormat("dd/MM/yyyy");
    // Permanent compact horizontal editor to preserve the table's height.
    QGridLayout *form = new QGridLayout(m_formVehicule);
    form->setContentsMargins(8,12,8,8); form->setHorizontalSpacing(10); form->setVerticalSpacing(5);
    auto addVehicleField = [&](int row, int col, const QString &name, QWidget *field) {
        QLabel *label = new QLabel(name); label->setStyleSheet("font-weight: normal;");
        QVBoxLayout *cell = new QVBoxLayout; cell->setContentsMargins(0,0,0,0); cell->setSpacing(2); cell->addWidget(label); cell->addWidget(field);
        QWidget *wrap = new QWidget; wrap->setLayout(cell); form->addWidget(wrap,row,col);
    };
    addVehicleField(0,0,"ID",m_vehicleId); addVehicleField(0,1,"Immatriculation",m_vehicleImmat); addVehicleField(0,2,"Type",m_vehicleType); addVehicleField(0,3,"Modèle",m_vehicleModele);
    addVehicleField(1,0,"État",m_vehicleEtat); addVehicleField(1,1,"Disponibilité",m_vehicleDispo); addVehicleField(1,2,"Date maintenance",m_vehicleDate);
    for (int c=0;c<4;++c) form->setColumnStretch(c,1);
    QPushButton *save = new QPushButton("Enregistrer"); save->setStyleSheet("QPushButton { background:#1F4E79; color:white; border-radius:5px; padding:6px 10px; font-weight:600; }");
    QPushButton *clear = new QPushButton("Effacer"); QHBoxLayout *actions = new QHBoxLayout; actions->addWidget(save); actions->addWidget(clear); form->addLayout(actions,1,3);
    ui->verticalLayoutVehiculesLeft->addWidget(m_formVehicule);
    // Keep the compact editor permanently visible below the table.
    m_formVehicule->show();
    QHBoxLayout *crud = ui->horizontalLayoutCrudVehicule;
    crud->removeWidget(ui->btnAjouterVehicule); crud->removeWidget(ui->btnModifierVehicule); crud->removeWidget(ui->btnSupprimerVehicule);
    ui->verticalLayoutVehiculesLeft->removeItem(crud);
    delete crud;
    QWidget *actionsCrud = new QWidget;
    QHBoxLayout *crudLayout = new QHBoxLayout(actionsCrud);
    crudLayout->setContentsMargins(0, 8, 0, 0);
    crudLayout->setSpacing(10);
    crudLayout->addWidget(ui->btnAjouterVehicule);
    crudLayout->addWidget(ui->btnModifierVehicule);
    crudLayout->addWidget(ui->btnSupprimerVehicule);
    crudLayout->addStretch(1);
    // Actions remain visible immediately below the vehicle table.
    ui->verticalLayoutVehiculesLeft->insertWidget(ui->verticalLayoutVehiculesLeft->indexOf(m_formVehicule), actionsCrud);
    connect(save, SIGNAL(clicked()), this, SLOT(enregistrerVehiculeInline())); connect(clear, SIGNAL(clicked()), this, SLOT(annulerVehiculeInline()));
    ui->btnStatsVehicule->hide(); ui->progressStatVehDispo->hide(); ui->labelStatVehDispo->hide(); ui->labelStatVehMaint->hide();
    m_vehiculeStatsChart = new PieChartWidget("VÉHICULES", ui->frameStatsVehicules);
    m_vehiculeStatsChart->setMinimumHeight(390);
    QScrollArea *statsScroll = new QScrollArea(ui->frameStatsVehicules);
    statsScroll->setWidgetResizable(true);
    statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    statsScroll->setFrameShape(QFrame::NoFrame);
    QWidget *statsContent = new QWidget;
    QVBoxLayout *statsContentLayout = new QVBoxLayout(statsContent);
    statsContentLayout->setContentsMargins(4, 4, 4, 4);
    statsContentLayout->addWidget(m_vehiculeStatsChart);
    statsScroll->setWidget(statsContent);
    ui->verticalLayoutStatsVehicules->insertWidget(1, statsScroll, 1);
    ui->frameStatsVehicules->setMinimumWidth(285);
    ui->frameStatsVehicules->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

// Données de démonstration (à remplacer plus tard par la base de données)
void GSmartFireStation::chargerVehiculesExemple()
{
    m_vehicules.clear();
    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("SELECT id_vehicule, immatriculation, type_vehicule, modele, etat, disponibilite, date_maintenance "
                  "FROM vehicule ORDER BY id_vehicule");
        if (q.exec()) {
            while (q.next()) {
                QString id = QString("V%1").arg(q.value(0).toInt(), 3, 10, QChar('0'));
                QString type = q.value(2).toString();
                if (type.compare("Camion-citerne", Qt::CaseInsensitive) == 0) type = "Citerne";
                QString etat = q.value(4).toString();
                if (etat.compare("Operationnel", Qt::CaseInsensitive) == 0 ||
                    etat.compare("Opérationnel", Qt::CaseInsensitive) == 0) etat = "Bon";
                if (!Vehicule::types().contains(type)) type = "Camion";
                if (!Vehicule::etats().contains(etat)) etat = "À contrôler";
                QDate date = q.value(6).toDate();
                if (!date.isValid()) date = QDate::currentDate().addMonths(3);
                m_vehicules.append(Vehicule(id, q.value(1).toString(), type,
                    q.value(3).toString(), etat, q.value(5).toString(), date));
            }
            return;
        }
        QMessageBox::warning(this, "Erreur Oracle",
            "Impossible de charger VEHICULE : " + q.lastError().text());
    }

    // Mode démonstration si Oracle n'est pas connecté.
    m_vehicules.append(Vehicule("V001", "123 TU 456", "Fourgon", "Renault", "Bon", "Disponible", QDate(2026, 11, 10)));
    m_vehicules.append(Vehicule("V002", "124 TU 457", "Camion", "Iveco", "Bon", "En mission", QDate(2026, 10, 22)));
    m_vehicules.append(Vehicule("V003", "125 TU 458", "Ambulance", "Mercedes", "À contrôler", "Disponible", QDate(2026, 10, 2)));
    m_vehicules.append(Vehicule("V004", "126 TU 459", "Citerne", "MAN", "Bon", "Disponible", QDate(2026, 12, 18)));
    m_vehicules.append(Vehicule("V005", "127 TU 460", "Ambulance", "Renault", "Bon", "Disponible", QDate(2027, 1, 15)));
    m_vehicules.append(Vehicule("V006", "128 TU 461", "Échelle", "Iveco", "À contrôler", "En maintenance", QDate(2026, 10, 29)));
    m_vehicules.append(Vehicule("V007", "129 TU 462", "Fourgon", "MAN", "Bon", "En maintenance", QDate(2026, 10, 12)));
}

// Numéro de la ligne sélectionnée dans le tableau (-1 si aucune)
int GSmartFireStation::vehiculeSelectionne()
{
    int ligne = ui->tableWidgetVehicules->currentRow();
    if (ligne < 0 || ligne >= m_vehicules.size())
        return -1;
    return ligne;
}

// Prochain ID libre : V001, V002, ...
QString GSmartFireStation::prochainIdVehicule()
{
    int max = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        int numero = m_vehicules[i].id().mid(1).toInt();
        if (numero > max)
            max = numero;
    }
    return QString("V%1").arg(max + 1, 3, 10, QChar('0'));
}

// ------------------------------------------------------------------ affichage
void GSmartFireStation::refreshVehiculesTable()
{
    int ligneCourante = ui->tableWidgetVehicules->currentRow();
    QColor noir("#1F1F1F");

    ui->tableWidgetVehicules->setRowCount(m_vehicules.size());
    for (int r = 0; r < m_vehicules.size(); r++) {
        Vehicule v = m_vehicules[r];

        // Couleur de l'état
        QColor couleurEtat = noir;
        bool etatGras = false;
        if (v.etat() == "À contrôler") {
            couleurEtat = QColor("#B45309");
            etatGras = true;
        } else if (v.etat() == "Hors service") {
            couleurEtat = QColor("#C00000");
            etatGras = true;
        }

        // Couleur de la disponibilité
        QColor couleurDispo("#C00000"); // En maintenance
        if (v.disponibilite() == "Disponible")
            couleurDispo = QColor("#2E7D32");
        else if (v.disponibilite() == "En mission")
            couleurDispo = QColor("#B45309");

        // Date de maintenance : orange si proche, rouge si dépassée
        QColor couleurDate = noir;
        bool dateGras = false;
        if (v.disponibilite() != "En maintenance" && v.maintenanceProche()) {
            dateGras = true;
            if (v.dateMaintenance() < QDate::currentDate())
                couleurDate = QColor("#C00000");
            else
                couleurDate = QColor("#B45309");
        }

        ui->tableWidgetVehicules->setItem(r, 0, creerItem(v.id(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 1, creerItem(v.immatriculation(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 2, creerItem(v.type(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 3, creerItem(v.modele(), noir, false));
        ui->tableWidgetVehicules->setItem(r, 4, creerItem(v.etat(), couleurEtat, etatGras));
        ui->tableWidgetVehicules->setItem(r, 5, creerItem(v.disponibilite(), couleurDispo, true));
        ui->tableWidgetVehicules->setItem(r, 6, creerItem(v.dateMaintenance().toString("dd/MM/yyyy"), couleurDate, dateGras));
    }
    if (ligneCourante >= 0 && ligneCourante < m_vehicules.size())
        ui->tableWidgetVehicules->selectRow(ligneCourante);

    filtrerVehicules();

    // Panneau "Statistiques véhicules"
    int dispo = 0;
    int maintenance = 0;
    for (int i = 0; i < m_vehicules.size(); i++) {
        if (m_vehicules[i].disponibilite() == "Disponible")
            dispo++;
        else if (m_vehicules[i].disponibilite() == "En maintenance")
            maintenance++;
    }
    int total = m_vehicules.size();
    int maximum = total;
    if (maximum < 1)
        maximum = 1;
    ui->labelStatVehDispo->setText(QString("Disponibles : %1 / %2").arg(dispo).arg(total));
    ui->progressStatVehDispo->setMaximum(maximum); ui->progressStatVehDispo->setValue(dispo);
    ui->labelStatVehMaint->setText(QString("En maintenance : %1").arg(maintenance));
    QStringList types = Vehicule::types(); QStringList labels; QList<int> values;
    for (int t=0; t<types.size(); ++t) { int count=0; for (int i=0;i<m_vehicules.size();++i) if (m_vehicules[i].type()==types[t]) ++count; labels << types[t]; values << count; }
    static_cast<PieChartWidget*>(m_vehiculeStatsChart)->setData(labels, values);
    refreshVehiculePropose();
}

// ------------------------------------------------------------------ CRUD
void GSmartFireStation::on_btnAjouterVehicule_clicked()
{
    annulerVehiculeInline(); m_vehiculeEditIndex = -1; m_vehicleId->setText(prochainIdVehicule());
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous"); m_formVehicule->show(); m_vehicleImmat->setFocus();
}

void GSmartFireStation::on_btnModifierVehicule_clicked()
{
    int i = vehiculeSelectionne();
    if (i < 0) { QMessageBox::information(this, "Modifier un véhicule", "Sélectionnez d'abord un véhicule dans la liste."); return; }
    const Vehicule v=m_vehicules[i]; m_vehiculeEditIndex=i; m_vehicleId->setText(v.id());
    m_vehicleImmat->setText(v.immatriculation()); m_vehicleType->setCurrentText(v.type()); m_vehicleModele->setText(v.modele());
    m_vehicleEtat->setCurrentText(v.etat()); m_vehicleDispo->setCurrentText(v.disponibilite()); m_vehicleDate->setDate(v.dateMaintenance());
    m_formVehicule->setTitle("Modification du véhicule " + v.id());
    m_formVehicule->show();
}

void GSmartFireStation::enregistrerVehiculeInline()
{
    QString immat = m_vehicleImmat->text().trimmed().toUpper();
    QString modele = m_vehicleModele->text().trimmed();
    if (immat.isEmpty() || modele.isEmpty()) {
        QMessageBox::warning(this, "Saisie incorrecte", "L'immatriculation et le modèle sont obligatoires.");
        return;
    }
    for (int i = 0; i < m_vehicules.size(); ++i) {
        if (i != m_vehiculeEditIndex && m_vehicules[i].immatriculation().compare(immat, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Doublon", "Cette immatriculation existe déjà.");
            return;
        }
    }

    const bool editing = m_vehiculeEditIndex >= 0 && m_vehiculeEditIndex < m_vehicules.size();
    if (DatabaseManager::isConnected()) {
        QString typeDb = m_vehicleType->currentText();
        if (typeDb == "Citerne") typeDb = "Camion-citerne";
        QString etatDb = m_vehicleEtat->currentText();
        if (etatDb == "Bon") etatDb = "Operationnel";

        QSqlQuery q(DatabaseManager::database());
        if (editing) {
            q.prepare("UPDATE vehicule SET immatriculation=:immat, type_vehicule=:type, modele=:modele, "
                      "etat=:etat, disponibilite=:dispo, date_maintenance=:date WHERE id_vehicule=:id");
            q.bindValue(":id", m_vehicules[m_vehiculeEditIndex].id().mid(1).toInt());
        } else {
            q.prepare("INSERT INTO vehicule (immatriculation, type_vehicule, modele, etat, disponibilite, date_maintenance) "
                      "VALUES (:immat, :type, :modele, :etat, :dispo, :date)");
        }
        q.bindValue(":immat", immat);
        q.bindValue(":type", typeDb);
        q.bindValue(":modele", modele);
        q.bindValue(":etat", etatDb);
        q.bindValue(":dispo", m_vehicleDispo->currentText());
        q.bindValue(":date", m_vehicleDate->date());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Enregistrement du véhicule impossible : " + q.lastError().text());
            return;
        }
        chargerVehiculesExemple();
        m_vehiculeEditIndex = -1;
        refreshVehiculesTable();
        m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
        statusBar()->showMessage(editing ? "Véhicule modifié dans Oracle." : "Véhicule ajouté dans Oracle.", 5000);
        return;
    }

    Vehicule v(editing ? m_vehicules[m_vehiculeEditIndex].id() : prochainIdVehicule(),
        immat, m_vehicleType->currentText(), modele, m_vehicleEtat->currentText(),
        m_vehicleDispo->currentText(), m_vehicleDate->date());
    if (editing) m_vehicules[m_vehiculeEditIndex] = v;
    else { m_vehicules.append(v); m_vehiculeEditIndex = m_vehicules.size() - 1; }
    refreshVehiculesTable();
    ui->tableWidgetVehicules->selectRow(m_vehiculeEditIndex);
    statusBar()->showMessage(editing ? "Véhicule modifié (mode démonstration)." : "Véhicule ajouté (mode démonstration).", 4000);
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
}

void GSmartFireStation::annulerVehiculeInline()
{
    m_vehiculeEditIndex=-1; m_vehicleId->setText(prochainIdVehicule()); m_vehicleImmat->clear(); m_vehicleModele->clear();
    m_vehicleType->setCurrentIndex(0); m_vehicleEtat->setCurrentIndex(0); m_vehicleDispo->setCurrentText("Disponible"); m_vehicleDate->setDate(QDate::currentDate().addMonths(3));
    m_formVehicule->setTitle("Fiche du véhicule — ajoutez ou modifiez les champs ci-dessous");
    m_formVehicule->show();
}

void GSmartFireStation::on_btnSupprimerVehicule_clicked()
{
    int i = vehiculeSelectionne();
    if (i < 0) {
        QMessageBox::information(this, "Supprimer un véhicule",
                                 "Sélectionnez d'abord un véhicule dans la liste.");
        return;
    }
    int reponse = QMessageBox::question(this, "Supprimer un véhicule",
                                        "Voulez-vous vraiment supprimer le véhicule "
                                        + m_vehicules[i].immatriculation() + " ?");
    if (reponse != QMessageBox::Yes)
        return;

    if (DatabaseManager::isConnected()) {
        QSqlQuery q(DatabaseManager::database());
        q.prepare("DELETE FROM vehicule WHERE id_vehicule=:id");
        q.bindValue(":id", m_vehicules[i].id().mid(1).toInt());
        if (!q.exec()) {
            QMessageBox::critical(this, "Erreur Oracle", "Suppression impossible : " + q.lastError().text());
            return;
        }
        chargerVehiculesExemple();
    } else {
        m_vehicules.removeAt(i);
    }
    ui->tableWidgetVehicules->setCurrentCell(-1, -1);
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule supprimé.", 5000);
}

void GSmartFireStation::on_tableWidgetVehicules_cellDoubleClicked(int, int)
{
    on_btnModifierVehicule_clicked(); // double-clic = modifier
}

// ------------------------------------------------------------------ recherche
// Cache les lignes qui ne contiennent pas le texte cherché (immatriculation, type, modèle ou ID)
void GSmartFireStation::filtrerVehicules()
{
    QString texte = ui->lineEditRechercheVehicule->text().trimmed();
    int visibles = 0;
    for (int r = 0; r < m_vehicules.size(); r++) {
        Vehicule v = m_vehicules[r];
        bool trouve = texte == ""
                      || v.immatriculation().contains(texte, Qt::CaseInsensitive)
                      || v.type().contains(texte, Qt::CaseInsensitive)
                      || v.modele().contains(texte, Qt::CaseInsensitive)
                      || v.id().contains(texte, Qt::CaseInsensitive);
        ui->tableWidgetVehicules->setRowHidden(r, !trouve);
        if (trouve)
            visibles++;
    }
    if (texte != "")
        statusBar()->showMessage(QString("%1 résultat(s)").arg(visibles), 4000);
    else
        statusBar()->clearMessage();
}

void GSmartFireStation::on_btnRechercherVehicule_clicked()
{
    filtrerVehicules();
}

void GSmartFireStation::on_lineEditRechercheVehicule_textChanged(const QString &)
{
    filtrerVehicules(); // la recherche se met à jour pendant la frappe
}

// ------------------------------------------------------------------ tri (tri à bulles)
// Retourne vrai si 'a' doit passer après 'b' selon le critère choisi
bool GSmartFireStation::doitEchangerVehicules(Vehicule a, Vehicule b, int critere)
{
    if (critere == 0) // État : Bon, À contrôler, Hors service
        return Vehicule::etats().indexOf(a.etat()) > Vehicule::etats().indexOf(b.etat());
    // Type : Fourgon, Camion, Ambulance, Citerne, Échelle
    return Vehicule::types().indexOf(a.type()) > Vehicule::types().indexOf(b.type());
}

void GSmartFireStation::on_btnTrierVehicule_clicked()
{
    int critere = ui->comboTriVehicule->currentIndex();
    int n = m_vehicules.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (doitEchangerVehicules(m_vehicules[j], m_vehicules[j + 1], critere)) {
                Vehicule temp = m_vehicules[j];
                m_vehicules[j] = m_vehicules[j + 1];
                m_vehicules[j + 1] = temp;
            }
        }
    }
    ui->tableWidgetVehicules->setCurrentCell(-1, -1);
    refreshVehiculesTable();
    statusBar()->showMessage("Liste triée.", 4000);
}

// ------------------------------------------------------------------ export PDF
void GSmartFireStation::on_btnExportVehicule_clicked()
{
    exporterTableauPdf(ui->tableWidgetVehicules, "Liste des véhicules", "vehicules.pdf");
}

// ------------------------------------------------------------------ statistiques
StatCritere GSmartFireStation::statVehiculesParEtat()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].etat();
    return compterParValeur("État", Vehicule::etats(), reelles);
}

StatCritere GSmartFireStation::statVehiculesParDispo()
{
    QStringList reelles;
    for (int i = 0; i < m_vehicules.size(); i++)
        reelles << m_vehicules[i].disponibilite();
    return compterParValeur("Disponibilité", Vehicule::disponibilites(), reelles);
}

void GSmartFireStation::on_btnStatsVehicule_clicked()
{
    // Les statistiques sont affichées en permanence dans le panneau latéral.
}

// ------------------------------------------------------------------ métier : choix adapté à la mission
// Propose un véhicule DISPONIBLE, en BON ÉTAT, dont le type correspond à l'intervention.
// On teste les types par ordre de préférence ; parmi les candidats on prend celui dont
// la prochaine maintenance est la plus lointaine.
void GSmartFireStation::refreshVehiculePropose()
{
    m_vehiculePropose = -1;
    ui->frameMission->hide();
    QStringList typesPreferes = Vehicule::typesPourIntervention(ui->comboIntervention->currentText());

    for (int t = 0; t < typesPreferes.size() && m_vehiculePropose < 0; t++) {
        int meilleur = -1;
        for (int i = 0; i < m_vehicules.size(); i++) {
            Vehicule v = m_vehicules[i];
            if (v.type() == typesPreferes[t] && v.disponibilite() == "Disponible" && v.etat() == "Bon") {
                if (meilleur < 0 || v.dateMaintenance() > m_vehicules[meilleur].dateMaintenance())
                    meilleur = i;
            }
        }
        m_vehiculePropose = meilleur; // reste -1 si aucun véhicule de ce type
    }

    if (m_vehiculePropose < 0) {
        ui->labelVehiculePropose->setText("Aucun véhicule adapté");
        ui->labelVehiculeInfo->setText("Aucun véhicule disponible en bon état pour cette intervention.");
        setLabelClass(ui->labelVehiculeInfo, "textWarning");
        ui->btnUtiliserVehicule->setEnabled(false);
        return;
    }

    Vehicule v = m_vehicules[m_vehiculePropose];
    ui->labelVehiculePropose->setText(v.id() + " — " + v.type() + " " + v.modele());
    ui->labelVehiculeInfo->setText(v.disponibilite() + " • État : " + v.etat());
    setLabelClass(ui->labelVehiculeInfo, "textSuccess");
    ui->btnUtiliserVehicule->setEnabled(true);
}

void GSmartFireStation::on_comboIntervention_currentIndexChanged(int)
{
    refreshVehiculePropose();
}

void GSmartFireStation::on_btnUtiliserVehicule_clicked()
{
    if (m_vehiculePropose < 0)
        return;

    Vehicule v = m_vehicules[m_vehiculePropose];
    QString intervention = ui->comboIntervention->currentText();
    int reponse = QMessageBox::question(this, "Affecter le véhicule",
                                        "Affecter " + v.id() + " (" + v.type() + " " + v.modele()
                                        + ") à l'intervention « " + intervention + " » ?");
    if (reponse != QMessageBox::Yes)
        return;

    m_vehicules[m_vehiculePropose].setDisponibilite("En mission");
    refreshVehiculesTable();
    statusBar()->showMessage("Véhicule " + v.id() + " affecté : " + intervention + ".", 6000);
}
