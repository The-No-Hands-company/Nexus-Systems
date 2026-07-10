#ifndef CATEGORYFILTER_H
#define CATEGORYFILTER_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStringList>
#include <QMap>

class CategoryFilter : public QWidget
{
    Q_OBJECT

public:
    explicit CategoryFilter(QWidget* parent = nullptr);
    ~CategoryFilter() override = default;

    enum SampleCategory {
        All = 0,
        Drums,
        Bass,
        Synth,
        Vocal,
        FX,
        Instrument,
        Loop,
        OneShot,
        Ambient,
        Percussion,
        CategoryCount
    };

    enum SampleGenre {
        AllGenres = 0,
        Electronic,
        HipHop,
        Rock,
        Pop,
        Jazz,
        Classical,
        Experimental,
        World,
        Cinematic,
        GenreCount
    };

signals:
    void filterChanged(SampleCategory category, SampleGenre genre, const QString& customFilter);

private slots:
    void onCategoryChanged(int index);
    void onGenreChanged(int index);
    void onClearFilters();

private:
    void setupUI();
    void setupCategories();

    QString getCategoryKeywords(SampleCategory category) const;
    QString getGenreKeywords(SampleGenre genre) const;
    QString getCustomFilter() const;

    QComboBox* categoryCombo;
    QComboBox* genreCombo;
    QPushButton* clearBtn;
    QLabel* activeFiltersLabel;

    SampleCategory currentCategory;
    SampleGenre currentGenre;

    QMap<SampleCategory, QStringList> categoryKeywords;
    QMap<SampleGenre, QStringList> genreKeywords;
};

#endif // CATEGORYFILTER_H