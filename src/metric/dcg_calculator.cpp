#include <LightGBM/metric.h>

#include <LightGBM/utils/log.h>

#include <cmath>

#include <vector>
#include <algorithm>

namespace LightGBM {

/*! \brief Declaration for some static members */
bool DCGCalculator::is_inited_ = false;
std::vector<double> DCGCalculator::label_gain_;
std::vector<double> DCGCalculator::discount_;
const data_size_t DCGCalculator::kMaxPosition = 10000;

void DCGCalculator::Init(std::vector<double> input_label_gain) {
  //  only inited one time
  if (is_inited_) { return; }
  label_gain_.clear();
  for(size_t i = 0;i < input_label_gain.size();++i){
    label_gain_.push_back(static_cast<double>(input_label_gain[i]));
  }
  label_gain_.shrink_to_fit();
  discount_.clear();
  for (data_size_t i = 0; i < kMaxPosition; ++i) {
    discount_.emplace_back(1.0 / std::log2(2.0 + i));
  }
  discount_.shrink_to_fit();
  is_inited_ = true;
}

double DCGCalculator::CalMaxDCGAtK(data_size_t k, const float* label, data_size_t num_data) {
  double ret = 0.0f;
  // counts for all labels
  std::vector<data_size_t> label_cnt(label_gain_.size(), 0);
  for (data_size_t i = 0; i < num_data; ++i) {
    ++label_cnt[static_cast<int>(label[i])];
  }
  int top_label = static_cast<int>(label_gain_.size()) - 1;

  if (k > num_data) { k = num_data; }
  //  start from top label, and accumulate DCG
  for (data_size_t j = 0; j < k; ++j) {
    while (top_label > 0 && label_cnt[top_label] <= 0) {
      top_label -= 1;
    }
    if (top_label < 0) {
      break;
    }
    ret += discount_[j] * label_gain_[top_label];
    label_cnt[top_label] -= 1;
  }
  return ret;
}

void DCGCalculator::CalMaxDCG(const std::vector<data_size_t>& ks,
                              const float* label,
                              data_size_t num_data,
                              std::vector<double>* out) {
  std::vector<data_size_t> label_cnt(label_gain_.size(), 0);
  // counts for all labels
  for (data_size_t i = 0; i < num_data; ++i) {
    if (static_cast<size_t>(label[i]) >= label_cnt.size()) { Log::Fatal("Label excel %d", label[i]); }
    ++label_cnt[static_cast<int>(label[i])];
  }
  double cur_result = 0.0f;
  data_size_t cur_left = 0;
  int top_label = static_cast<int>(label_gain_.size()) - 1;
  // calculate k Max DCG by one pass
  for (size_t i = 0; i < ks.size(); ++i) {
    data_size_t cur_k = ks[i];
    if (cur_k > num_data) { cur_k = num_data; }
    for (data_size_t j = cur_left; j < cur_k; ++j) {
      while (top_label > 0 && label_cnt[top_label] <= 0) {
        top_label -= 1;
      }
      if (top_label < 0) {
        break;
      }
      cur_result += discount_[j] * label_gain_[top_label];
      label_cnt[top_label] -= 1;
    }
    (*out)[i] = cur_result;
    cur_left = cur_k;
  }
}


double DCGCalculator::CalDCGAtK(data_size_t k, const float* label,
                                const double* score, data_size_t num_data) {
  // get sorted indices by score
  std::vector<data_size_t> sorted_idx;
  for (data_size_t i = 0; i < num_data; ++i) {
    sorted_idx.emplace_back(i);
  }
  std::sort(sorted_idx.begin(), sorted_idx.end(),
           [score](data_size_t a, data_size_t b) {return score[a] > score[b]; });

  if (k > num_data) { k = num_data; }
  double dcg = 0.0f;
  // calculate dcg
  for (data_size_t i = 0; i < k; ++i) {
    data_size_t idx = sorted_idx[i];
    dcg += label_gain_[static_cast<int>(label[idx])] * discount_[i];
  }
  return dcg;
}

void DCGCalculator::CalDCG(const std::vector<data_size_t>& ks, const float* label,
                           const double * score, data_size_t num_data, std::vector<double>* out) {
  // get sorted indices by score
  std::vector<data_size_t> sorted_idx;
  for (data_size_t i = 0; i < num_data; ++i) {
    sorted_idx.emplace_back(i);
  }
  std::sort(sorted_idx.begin(), sorted_idx.end(),
            [score](data_size_t a, data_size_t b) {return score[a] > score[b]; });

  double cur_result = 0.0f;
  data_size_t cur_left = 0;
  // calculate multi dcg by one pass
  for (size_t i = 0; i < ks.size(); ++i) {
    data_size_t cur_k = ks[i];
    if (cur_k > num_data) { cur_k = num_data; }
    for (data_size_t j = cur_left; j < cur_k; ++j) {
      data_size_t idx = sorted_idx[j];
      cur_result += label_gain_[static_cast<int>(label[idx])] * discount_[j];
    }
    (*out)[i] = cur_result;
    cur_left = cur_k;
  }
}

}  // namespace LightGBM
