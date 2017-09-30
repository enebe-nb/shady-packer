#include "../Core/converter.hpp"
#include "../Core/resource/readerwriter.hpp"
#include "util.hpp"
#include <fstream>
#include <iostream>
#include <cinttypes>

class ConverterSuite : public CxxTest::TestSuite {
public:
    void testConverters() {
        const struct {
            const char* inputFile;
            const char* outputFile;
            ShadyCore::Converter* convert;
        } dataArray[] = {
            { "test-data/decrypted/data/my-text.txt", "test-data/encrypted/data/my-text.cv0", ShadyCore::TextConverter },
            { "test-data/encrypted/data/my-text.cv0", "test-data/decrypted/data/my-text.txt", ShadyCore::TextConverter },
            { "test-data/decrypted/data/my-text.csv", "test-data/encrypted/data/my-text.cv1", ShadyCore::TextConverter },
            { "test-data/encrypted/data/my-text.cv1", "test-data/decrypted/data/my-text.csv", ShadyCore::TextConverter },
			//{ "test-data/decrypted/data/my-image.png", "test-data/encrypted/data/my-image.cv2", ShadyCore::ImageEncrypter },
			//{ "test-data/decrypted/data/my-image-indexed.png", "test-data/encrypted/data/my-image-indexed.cv2", ShadyCore::ImageEncrypter },
			//{ "test-data/encrypted/data/my-image.cv2", "test-data/decrypted/data/my-image.png", ShadyCore::ImageDecrypter },
			//{ "test-data/encrypted/data/my-image-indexed.cv2", "test-data/decrypted/data/my-image-indexed.png", ShadyCore::ImageDecrypter },
			{ "test-data/decrypted/data/my-sfx.wav", "test-data/encrypted/data/my-sfx.cv3", ShadyCore::SfxEncrypter },
			{ "test-data/encrypted/data/my-sfx.cv3", "test-data/decrypted/data/my-sfx.wav", ShadyCore::SfxDecrypter },
			//{ "test-data/decrypted/data/my-gui.gui", "test-data/encrypted/data/my-gui.dat", ShadyCore::GuiEncrypter },
			//{ "test-data/encrypted/data/my-gui.dat", "test-data/decrypted/data/my-gui.gui", ShadyCore::GuiDecrypter },
			//{ "test-data/decrypted/data/my-effect.xml", "test-data/encrypted/data/my-effect.pat", ShadyCore::PatternEncrypter },
			//{ "test-data/decrypted/data/my-pattern.xml", "test-data/encrypted/data/my-pattern.pat", ShadyCore::PatternEncrypter },
			//{ "test-data/encrypted/data/my-effect.pat", "test-data/decrypted/data/my-effect.xml", ShadyCore::PatternAnimationDecrypter },
			//{ "test-data/encrypted/data/my-pattern.pat", "test-data/decrypted/data/my-pattern.xml", ShadyCore::PatternDecrypter },
			//{ "test-data/decrypted/data/my-palette.act", "test-data/encrypted/data/my-palette.pal", ShadyCore::PaletteEncrypter },
			//{ "test-data/encrypted/data/my-palette.pal", "test-data/decrypted/data/my-palette.act", ShadyCore::PaletteDecrypter },
			{ "test-data/decrypted/data/my-label.lbl", "test-data/encrypted/data/my-label.sfl", ShadyCore::SflEncrypter },
			{ "test-data/encrypted/data/my-label.sfl", "test-data/decrypted/data/my-label.lbl", ShadyCore::SflDecrypter },
		};

        for (auto data : dataArray) {
            std::ifstream input(data.inputFile, std::ios::binary);
            std::ifstream output(data.outputFile, std::ios::binary);
			std::stringstream tempStream;

            data.convert(input, tempStream);
            //TS_ASSERT_STREAM(tempStream, output);

            input.close();
            output.close();
        }
    }

	void testReaderWriter() {
		const char* dataArray[][2] = {
			{ "test-data/encrypted/data/my-text.cv0", "test-data/decrypted/data/my-text.txt" },
			{ "test-data/encrypted/data/my-text.cv1", "test-data/decrypted/data/my-text.csv" },
			{ "test-data/encrypted/data/my-image.cv2", "test-data/decrypted/data/my-image.png" },
			{ "test-data/encrypted/data/my-image-indexed.cv2", "test-data/decrypted/data/my-image-indexed.png" },
			{ "test-data/encrypted/data/my-palette.pal", "test-data/decrypted/data/my-palette.act" },
			{ "test-data/encrypted/data/my-label.sfl", "test-data/decrypted/data/my-label.lbl" },
			{ "test-data/encrypted/data/my-sfx.cv3", "test-data/decrypted/data/my-sfx.wav" },
            { "test-data/encrypted/data/my-gui.dat", "test-data/decrypted/data/my-gui.gui" },
            { "test-data/encrypted/data/my-pattern.pat", "test-data/decrypted/data/my-pattern.xml" },
            { "test-data/encrypted/data/my-effect.pat", "test-data/decrypted/data/my-effect.xml" },
		};

		for (auto data : dataArray) {
			for (int i = 0; i < 2; ++i) {
				std::ifstream input(data[i], std::ios::binary), expected;
				const ShadyCore::FileType& type = ShadyCore::FileType::get(data[i], input);
				ShadyCore::Resource* resource = type.createResource();
                ShadyCore::ResourceReader(resource, input, type.isEncrypted);

				std::stringstream output;
				ShadyCore::ResourceWriter(resource, output, true);
				expected.open(data[0], std::ios::binary);
				TS_ASSERT_STREAM(output, expected);
				expected.close();

				output.clear(); output.str("");
				ShadyCore::ResourceWriter(resource, output, false);
				expected.open(data[1], std::ios::binary);
				TS_ASSERT_STREAM(output, expected);
				expected.close();

                std::ofstream o("test.txt", std::ios::binary);
                ShadyCore::ResourceWriter(resource, o, false);

				input.close();
			}
		}
	}

    void testZ() {
        const char* a[] = {
            "/home/becker/neto/th123data/character/alice/alice.pat",
            "/home/becker/neto/th123data/character/aya/aya.pat",
            "/home/becker/neto/th123data/character/chirno/chirno.pat",
            "/home/becker/neto/th123data/character/iku/iku.pat",
            "/home/becker/neto/th123data/character/komachi/komachi.pat",
            "/home/becker/neto/th123data/character/marisa/marisa.pat",
            "/home/becker/neto/th123data/character/meirin/meirin.pat",
            "/home/becker/neto/th123data/character/patchouli/patchouli.pat",
            "/home/becker/neto/th123data/character/reimu/reimu.pat",
            "/home/becker/neto/th123data/character/remilia/remilia.pat",
            "/home/becker/neto/th123data/character/sakuya/sakuya.pat",
            "/home/becker/neto/th123data/character/sanae/sanae.pat",
            "/home/becker/neto/th123data/character/suika/suika.pat",
            "/home/becker/neto/th123data/character/suwako/suwako.pat",
            "/home/becker/neto/th123data/character/tenshi/tenshi.pat",
            "/home/becker/neto/th123data/character/udonge/udonge.pat",
            "/home/becker/neto/th123data/character/utsuho/utsuho.pat",
            "/home/becker/neto/th123data/character/youmu/youmu.pat",
            "/home/becker/neto/th123data/character/yukari/yukari.pat",
            "/home/becker/neto/th123data/character/yuyuko/yuyuko.pat",
        };
        for (auto data : a) {
        /*
            std::ifstream input(data, std::ios::binary);
            ShadyCore::Pattern* resource = new ShadyCore::Pattern();
            resource->initialize(false);
            printf("%s\n", data);
            ShadyCore::ResourceReader(resource, input, true);

            for (int i = 0; i < resource->getSequenceCount(); ++i) {
                if (resource->getSequence(i).getType() == ShadyCore::Sequence::SEQUENCE_CLONE) continue;
                ShadyCore::Sequence::Move& seq = resource->getSequence(i).asMove();
                for (int j = 0; j < seq.getFrameCount(); ++j) {
                    ShadyCore::Frame::Move& frame = seq.getFrame(j).asMove();
                    if (frame.getUnknown()) {
                        printf("Frame(%d/%d/%d): %" PRIu16 "\n", seq.getId(), seq.getIndex(), frame.getIndex(), frame.getUnknown());
                    }
                    if (frame.getEffect().getUnknown01()) {
                        printf("Eff01(%d/%d/%d): %" PRIu64 "\n", seq.getId(), seq.getIndex(), frame.getIndex(), frame.getEffect().getUnknown01());
                    }
                    if (frame.getEffect().getUnknown02()) {
                        printf("Eff02(%d/%d/%d): %" PRIu16 "\n", seq.getId(), seq.getIndex(), frame.getIndex(), frame.getEffect().getUnknown02());
                    }
                }
            }

            delete resource;
            system("read");
            */
        }
	}
};
