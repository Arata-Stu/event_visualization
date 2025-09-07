#include "hdf5_loader.h"
#include <iostream>
#include <H5DataSpace.h>
#include <H5DataType.h>

HDF5Loader::HDF5Loader(const std::string& filepath)
    : file(filepath, H5F_ACC_RDONLY) {
    std::cout << "HDF5Loader: " << filepath << " を開きました。" << std::endl;
}

HDF5Loader::~HDF5Loader() {
    std::cout << "HDF5Loader: ファイルを閉じました。" << std::endl;
}

int64_t HDF5Loader::load_t_offset() {
    try {
        H5::DataSet dset = file.openDataSet("/t_offset");
        int64_t offset = 0;
        dset.read(&offset, H5::PredType::NATIVE_INT64);
        std::cout << "--- t_offset: " << offset << " を読み込みました ---" << std::endl;
        return offset;
    } catch (H5::Exception& err) {
        std::cerr << "t_offsetの読み込みに失敗しました。デフォルト値0を使用します。" << std::endl;
        return 0;
    }
}

std::vector<EventCD> HDF5Loader::load_all_events() {
    try {
        H5::DataSet x_dset = file.openDataSet("/events/x");
        size_t num_events = x_dset.getSpace().getSelectNpoints();

        if (num_events == 0) {
            std::cout << "イベントデータが空です。" << std::endl;
            return {};
        }

        std::cout << "--- " << num_events << " 個のイベントを読み込み開始..." << std::endl;
        std::vector<uint16_t> x_vec(num_events), y_vec(num_events);
        std::vector<uint32_t> t_vec(num_events);
        std::vector<uint8_t> p_vec(num_events);

        x_dset.read(x_vec.data(), H5::PredType::NATIVE_UINT16);
        file.openDataSet("/events/y").read(y_vec.data(), H5::PredType::NATIVE_UINT16);
        file.openDataSet("/events/t").read(t_vec.data(), H5::PredType::NATIVE_UINT32);
        file.openDataSet("/events/p").read(p_vec.data(), H5::PredType::NATIVE_UINT8);
        std::cout << "--- HDF5からの読み込み完了 ---" << std::endl;

        std::vector<EventCD> events(num_events);
        for (size_t i = 0; i < num_events; ++i) {
            events[i] = {x_vec[i], y_vec[i], p_vec[i], t_vec[i]};
        }
        
        std::cout << "--- イベントデータを構造体にまとめました ---" << std::endl;
        return events;

    } catch (H5::Exception& err) {
        std::cerr << "HDF5読み込みエラー:" << std::endl;
        err.printErrorStack();
        return {};
    }
}