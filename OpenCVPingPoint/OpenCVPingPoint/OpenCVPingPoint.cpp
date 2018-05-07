#define _CRT_SECURE_NO_WARNINGS // Pomijanie błędu kompilacji o funkcji sscanf

#include <stdio.h> // sscanf, printf
#include <windows.h> // Obsługa potoków, GetKeyState
#include <stdlib.h>  // strtol
#include "opencv2/highgui.hpp" // Wyświetlanie okien
#include "opencv2/imgproc.hpp" // Operacje na obrazie
#include <fstream> // Zapis współrzędnych do pliku

using namespace cv;
using namespace std;

				//~Zmienne globalne~//

VideoCapture capture; // Obiekt służący do przechwytywania kamery/pliku video

Mat video; // Obiekt surowych klatek z kamery/pliku video

Mat linie; // Obiekt obrazu na którym rysujemy linię piłki

Mat klatka; // Obiekt klatki wizualizującej kalibrację stołu

Mat3b okno_przycisku; // Obiekt okna zawierającego przycsik zliczania punktów

Rect przycisk; // Obiekt reprezentująct przycisk zliczania punktów

char bufor[100]; // Bufor trzymający numer strony otrzymującej punkt, jej rozmiar musi być identyczny jak w aplikacji C# (0 - lewa strona stołu 1 - prawa strona stołu)

DWORD cbWritten; // Nieużywana zmienna wymagana do działania funkcji WriteLine

LPCTSTR NazwaPotoku = TEXT("\\\\.\\pipe\\myNamedPipe1"); // Nazwa potoku wysyłania numeru strony do aplikacji C# - musi być identyczna w obu aplikacjach

HANDLE Potok = CreateFile(NazwaPotoku, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL); // Obiekt tworzący potok

Mat videoHSV; // Obiekt obrazu w postaci HSV do kalibracja piłki

Mat videoProgowe; // Obiekt obrazu czarno-białego służący do kalibracji piłki

int IDKamery; // liczba symbolizująca ID Kamery podany w wierszu poleceń

bool GrajVideo = true; // Boolowska zmienna służąca do zatrzymywania pliku video za pomocą przycisku "p"

bool skalibrowano = FALSE; // Boolowska zmienna mowiąca czy stół został skalibrowany

bool LiczPunkty = FALSE; // Boolowska zmienna mowiąca czy należy rozpocząć liczenie punktów

// Ustalone wartości HSV
int LowH = 0;
int LowS = 0;
int LowV = 0;

int HighH = 30;
int HighS = 255;
int HighV = 255;

// Zmienne ostatniej pozycji piłki służące do rysowania linii, początkowo nie znamy pozycji piłki, więc są zerowe
double ostatnieX = -1;
double ostatnieY = -1;

ofstream wspol; // Obiekt zapisywania współrzędnych do pliku

// Punkty otrzymane w wyniku kalibracji stołu
Point w_1; // Lewy wierzchołek stołu
Point w_2; // Prawy wierzchołek stołu
Point w_x; // Środek stołu (pozycja siatki)

int i = 0; // Licznik kliknięć okna kalibracji stołu


void WyslijStrone(const char *strona) // Funkcja wysyłająca numer strone do aplikacji C# Ping Point
{
	sscanf(strona, "%s", bufor); // Wczytaj stronę do buforu
	// Zapisz stronę i wyślij do aplikacji C#
	WriteFile(Potok, bufor, 1, &cbWritten, NULL);
	memset(bufor, 0xCC, 100);
}

void kalibracja_stolu(int event, int x, int y, int flags, void* userdata) // Funkcja kalibracji stołu
{
	if (event == EVENT_LBUTTONDOWN && !skalibrowano) // Wykrywanie klikniecie lewym przyciskiem myszy w okno kalibracji stołu w celu jego kalibracji
	{
		klatka = video.clone(); // Bierzemy świeżą klatkę bez tekstu

		if (i == 0)
		{	
			klatka = video.clone(); // Bierzemy świeżą klatkę bez tekstu

			w_1 = Point(x, y); // Zapisujemy pierwszy punkt

			putText(klatka, "Wskaz prawy wierzcholek stolu.", Point(25, 25), FONT_HERSHEY_TRIPLEX, 1, Scalar(0, 0, 0), 1, LINE_AA); // Wyświetl tekst

			imshow("Kalibracja stolu", klatka); // Zmień klatkę
		}

		if (i == 1)
		{
			klatka = video.clone(); // Bierzemy świeżą klatkę bez tekstu

			w_2 = Point(x, y); // Zapisujemy drugi punkt

			line(klatka, w_1, w_2, Scalar(255, 255, 255), 2); // Rysujemy linię

			putText(klatka, "Skalibrowano!", Point(25, 25), FONT_HERSHEY_TRIPLEX, 1, Scalar(0, 0, 0), 1, LINE_AA); // Wyświetl tekst

			// Obliczamy środek stołu
			w_x.x = (w_1.x + w_2.x) / 2;
			w_x.y = (w_1.y + w_2.y) / 2;

			// Ryzujemy kółka poglądowe:
			circle(klatka, w_1, 10, Scalar(0, 0, 255), -1); // czerwone - lewa strona
			circle(klatka, w_2, 10, Scalar(0, 255, 255), -1); // żółte - prawa strona
			circle(klatka, w_x, 10, Scalar(0, 255, 0), -1); // zielone - środek (patrzymy czy zgadza się z faktycznym środkiem stołu/siatką

			skalibrowano = TRUE; // Kończymy kalibrację

			imshow("Kalibracja stolu", klatka); // Zmień klatkę
		}

		i++; // Zwiększamy licznik kliknięć

	}

	if (event == EVENT_RBUTTONDOWN && skalibrowano) // Wykrywanie klikniecie prawym przyciskiem myszy w okno kalibracji stołu w celu usunięcią kalibracji
	{
		i = 0; // Zerowanie licznika kliknięć

		// Zerowanie współrzędnych punktów
		w_1 = Point(0, 0); 
		w_2 = Point(0, 0);
		w_x = Point(0, 0);

		LiczPunkty = FALSE; // Blokujemy liczenie punktów

		klatka = video.clone(); // bierzemy świeżą klatkę bez tekstu

		putText(klatka, "Wskaz lewy wierzcholek stolu.", Point(25, 25), FONT_HERSHEY_TRIPLEX, 1, Scalar(0, 0, 0), 1, LINE_AA); // Wyświetl tekst

		imshow("Kalibracja stolu", klatka); // Zmień klatkę

		skalibrowano = FALSE; // Ponów kalibrację
	}

}

int main(int argc, char *argv[])
{
	//~Sprawdzamy poprawność argumentów wiersza poleceń programu~//
	if (argc > 2)
	{
		if (strcmp("k", argv[1]) == 0) // Tryb kamery na żywo
		{
			IDKamery = strtol(argv[2], NULL, 10); // Pozyskwanie ID kamery z wiersza poleceń
			capture.open(IDKamery); // Otwieramy kamerę
		}
		else if (strcmp("v", argv[1]) == 0) // Tryb filmu
		{
			capture.open(argv[2]); // Otwieramy film 
		}
	}

	else
	{
		printf("Podaj tryb pracy, oraz ID Kamery/Sciezke do pliku.\n");
		return -1;
	}

	if (!capture.isOpened()) // Sprawdzamy czy poprawnie otwarto kamerę/plik
	{
		printf("Podaj ID Kamery, lub podlacz kamere do komputera, lub sprawdz sciezke do pliku.\n\n");
		return -1;
	}

	//~Okno kalibracji stołu~//

	capture.read(klatka); // Przechwytywanie tymczasowej, pierwszej klatki, w celu utworzenia okna kalibracji stołu

	namedWindow("Kalibracja stolu", WINDOW_NORMAL); // Tworzenie okna kalibracji stołu

	setMouseCallback("Kalibracja stolu", kalibracja_stolu); // Aktywacja funkcji "kalibracja_stolu"

	putText(klatka, "Wskaz lewy wierzcholek stolu.", Point(25, 25), FONT_HERSHEY_TRIPLEX, 1, Scalar(0, 0, 0), 1, LINE_AA); // Wyświetl tekst

	imshow("Kalibracja stolu", klatka); // Pokaż okna kalibracji

	//~Okno kalibracji piłki~//

	namedWindow("Kalibracja pilki", WINDOW_NORMAL); // Tworzenia okna kalibracji piłki

	// Tworzenie suwaków do kalibracji piłki
	createTrackbar("Low H", "Kalibracja pilki", &LowH, 179);
	createTrackbar("High H", "Kalibracja pilki", &HighH, 179);

	createTrackbar("Low S", "Kalibracja pilki", &LowS, 255);
	createTrackbar("High S", "Kalibracja pilki", &HighS, 255);

	createTrackbar("Low V", "Kalibracja pilki", &LowV, 255);
	createTrackbar("High V", "Kalibracja pilki", &HighV, 255);

	//~Okno przycisku liczenia punktu TODO~//

	//przycisk = Rect(0, 0, 50, 50); // Tworzenie przycisku

	//okno_przycisku = Mat3b(img.rows + button.height, img.cols, Vec3b(0, 0, 0));

	//namedWindow("Liczenie puntkow", WINDOW_NORMAL);

	//imshow("Liczenie puntkow", przycisk);

	//~Pozostałe procedury~//
	
	wspol.open("wspolrzedne.txt"); // Tworzymy plik ze współrzędnymi i na nim piszemy

	namedWindow("Obraz na zywo z linia toru pilki", WINDOW_NORMAL); // Tworzenie okna obrazu na żywo z linią toru piłki

	//~~Główna pętla programu: obraz na żywo, obraz kalibracji piłki na żywo, rysowanie toru piłki na żywo, zliczanie punktów na żywo~~//

	if ((Potok == NULL || Potok == INVALID_HANDLE_VALUE)) // Sprawdzanie utworzenia potoku
	{
		printf("Blad tworzenia potoku. Sprawdz czy aplikacja Ping Point C# jest uruchomiona.\n");
		return -1;
	}

	else
	{
		do
		{
			if (waitKey(1) == 27) break; // Ciągle rysuj nowe klatki, oraz zakończ aplikację gdy naciśniemy klawisz ESC

			int64 start = getTickCount(); // Zmienna służąca do wypisywania liczby klatek na sekundę do konsoli

			if ((GetKeyState('P') & 0x8000) && (strcmp("v", argv[1]) == 0)) GrajVideo = !GrajVideo; // Zatrzymaj/ponów plik video za pomocą przycisku "p"
				
			if (GrajVideo) capture.read(video); // Ciagle przechwytuj nową klatkę kamery

			//~Kalibracja piłki~//

			cvtColor(video, videoHSV, COLOR_BGR2HSV); // Konwersja klatki do postaci HSV (Hue, Saturation, Value) w celu kalibracji piłki

			inRange(videoHSV, Scalar(LowH, LowS, LowV), Scalar(HighH, HighS, HighV), videoProgowe); 

			// Operacje usuwające szum: rozmycie Gaussa, erozja, dylatacja
			GaussianBlur(videoProgowe, videoProgowe, Size(25, 25), 0);
			erode(videoProgowe, videoProgowe, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(videoProgowe, videoProgowe, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			dilate(videoProgowe, videoProgowe, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
			erode(videoProgowe, videoProgowe, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

			// Szukanie okręgu piłki i jego środka

			imshow("Kalibracja pilki", videoProgowe); // Dodanie i wyświetlanie obrazu czarno-białego do kalibracji piłki w jego oknie

			//~Wykrywanie środka wykrytej wykrytej piłki oraz rysowanie jej toru~//

			linie = Mat::zeros(video.size(), CV_8UC3); // Tworzymy pusty obraz gdzie będziemy rysować linię toru piłki
			
			Moments momenty = moments(videoProgowe); // Moments - średnia ważona intensywności pikseli lub ich funkcja, preferowana metoda wykrywania środka dowolnego obiektu, tworzymy je z czarno-białego obrazu kalibracji piłki

			double m01 = momenty.m01;
			double m10 = momenty.m10;
			double mPowierzchnia = momenty.m00;

			double X = m10 / mPowierzchnia;
			double Y = m01 / mPowierzchnia;

			if (ostatnieX >= 0 && ostatnieY >= 0 && X >= 0 && Y >= 0) line(linie, Point((int)X, (int)Y), Point((int)ostatnieX, (int)ostatnieY), Scalar(0, 255, 0), 4, LINE_AA); // Rysujemy linię gdy piłka jest w zasięgu obrazu
			wspol << " X: " << (int)X << " Y: " << (int)Y << "\n"; // Zapisujemy współrzędne do pliku

			// Koniec poprzedniej linii staje się początkiem nowej
			ostatnieX = X;
			ostatnieY = Y;

			imshow("Obraz na zywo z linia toru pilki", video + linie); //pokazywanie obrazu na żywo wraz z nałożonym obrazem toru linii piłki			

			// Liczenie i wypisywanie FPS
			double fps = getTickFrequency() / (getTickCount() - start); 
			printf("FPS: %f\n", fps);
			

		} while (1);

		//~Operacje po zakończeniu programu~/

		wspol.close(); // Zamknięcie pliku
		CloseHandle(Potok); // Zamknięcie potoku
	}

	return 0;
}

